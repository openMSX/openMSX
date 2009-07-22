// $Id$

#include "OggReader.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
#include "yuv2rgb.hh"
#include "likely.hh"
#include <cstring>
#include <cstdlib>

namespace openmsx {

OggReader::OggReader(const std::string& filename)
{
	LocalFileReference file(filename);
	oggz = oggz_open(file.getFilename().c_str(), OGGZ_READ | OGGZ_AUTO);
	if (!oggz) {
		throw MSXException("Failed to open " + filename);
	}

	pcm[0] = pcm[1] = 0;
	pcmSize = 0;
	readPos = writePos = 0;
	audio_serial = -1;
	video_serial = -1;
	video_header_packets = 3;
	audio_header_packets = 3;

	oggz_set_read_callback(oggz, -1, readCallback, this);

	theora_info_init(&video_info);
	theora_comment_init(&video_comment);
	yuv_valid = false;

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	while (audio_header_packets || video_header_packets) {
		if (oggz_read(oggz, 1024) <= 0) {
			break;
		}
	}

	if (video_serial == -1) {
		cleanup();
		throw MSXException("No video track found");
	}

	if (audio_serial == -1) {
		cleanup();
		throw MSXException("No audio track found");
	}

	if (vi.channels != 2) {
		cleanup();
		throw MSXException("Audio must be stereo");
	}

	if (video_info.width != 640 || video_info.height != 480) {
		cleanup();
		throw MSXException("Video must be 640x480");
	}

	if (video_info.fps_numerator != 30000 ||
	    video_info.fps_denominator != 1001) {
		cleanup();
		throw MSXException("Video must be 29.97Hz");
	}
}

void OggReader::cleanup()
{
	free(pcm[0]);
	free(pcm[1]);

	if (audio_header_packets == 0) {
		vorbis_dsp_clear(&vd);
		vorbis_block_clear(&vb);
	}

	if (video_header_packets == 0) {
		theora_clear(&video_handle);
	}

	vorbis_info_clear(&vi);
	vorbis_comment_clear(&vc);

	theora_info_clear(&video_info);
	theora_comment_clear(&video_comment);

	oggz_close(oggz);
}

OggReader::~OggReader()
{
	cleanup();
}

int OggReader::readCallback(OGGZ* /*oggz*/, ogg_packet* packet,
                            long serial, void* userdata)
{
	OggReader* reader = static_cast<OggReader*>(userdata);

	if (unlikely(packet->b_o_s && packet->bytes >= 8)) {
		if (memcmp(packet->packet, "\001vorbis", 7) == 0) {
			reader->audio_serial = serial;
                } else if (memcmp(packet->packet, "\200theora", 7) == 0) {
			reader->video_serial = serial;
		}
	}

	if (reader->video_serial == serial) {
		reader->readTheora(packet);
	} else if (reader->audio_serial == serial) {
		reader->readVorbis(packet);
	}

	return 0;
}

int OggReader::seekCallback(OGGZ* /*oggz*/, ogg_packet* packet,
                            long serial, void* userdata)
{
	OggReader* reader = static_cast<OggReader*>(userdata);

	if (reader->video_serial == serial && reader->intraframes == -1) {
		reader->intraframes = packet->granulepos &
			((1 << oggz_get_granuleshift(reader->oggz, serial)) - 1);
	}

	return 0;
}


void OggReader::readVorbis(ogg_packet* packet)
{
	if (unlikely(audio_header_packets)) {
		if (vorbis_synthesis_headerin(&vi, &vc, packet) < 0) {
			return;
		}
		if (!--audio_header_packets) {
			vorbis_synthesis_init(&vd, &vi);
			vorbis_block_init(&vd, &vb);
		}
		return;
	}

	if (vorbis_synthesis(&vb, packet) != 0) {
		return;
	}

	vorbis_synthesis_blockin(&vd, &vb);

	if (readPos == writePos) {
		readPos = 0 ;
		writePos = 0;
	}

	float** dpcm;
	long decoded;
	for (/**/; (decoded = vorbis_synthesis_pcmout(&vd, &dpcm)); /**/) {
		if (writePos + decoded > pcmSize) {
			long size = writePos + decoded + 4096;
			pcm[0] = static_cast<float*>(realloc(pcm[0], size * sizeof(float)));
			pcm[1] = static_cast<float*>(realloc(pcm[1], size * sizeof(float)));
			pcmSize = size;
		}

		for (long i = 0; i < decoded; ++i) {
			pcm[0][writePos + i] = dpcm[0][i];
			pcm[1][writePos + i] = dpcm[1][i];
		}
		writePos += decoded;

		vorbis_synthesis_read(&vd, decoded);
	}
}

void OggReader::readTheora(ogg_packet* packet)
{
	if (unlikely(theora_packet_isheader(packet))) {
		if (theora_decode_header(&video_info, &video_comment,
		                          packet) < 0) {
			return;
		}
		if (--video_header_packets == 0) {
			theora_decode_init(&video_handle, &video_info);
		}
		return;

	} else if (video_header_packets) {
		return;
	}

	yuv_valid = false;
	if (theora_decode_packetin(&video_handle, packet) < 0) {
		return;
	}
	if (theora_decode_YUVout(&video_handle, &yuv_frame) < 0) {
		return;
	}
	yuv_valid = true;
}

void OggReader::getFrame(RawFrame& result)
{
	// TODO When exactly does yuv_frame contain valid data?
	//      Are the assignments to 'yuv_valid' in the method above correct?
	if (yuv_valid) {
		yuv2rgb::convert(yuv_frame, result);
	}
}

bool OggReader::seek(unsigned pos)
{
	readPos = 0;
	writePos = 0;

	ogg_int64_t newpos = oggz_seek_units(oggz, pos, SEEK_SET);
	if (newpos == -1) {
		return false;
	}

	oggz_set_read_callback(oggz, -1, seekCallback, this);

	intraframes = -1;
	do {
		if (oggz_read(oggz, 4096) <= 0) {
			break;
		}
	} while (intraframes == -1);

	oggz_set_read_callback(oggz, -1, readCallback, this);

	// Seek to the keyframe, two extra for rounding errors
	ogg_int64_t keyframe = pos - (intraframes + 2) * 1001 / 30;
	newpos = oggz_seek_units(oggz, keyframe < 0 ? 0 : keyframe, SEEK_SET);

	vorbis_synthesis_restart(&vd);

	if (newpos >= 0 && newpos < pos) {
		unsigned samples = (pos - newpos) * vi.rate / 1000;
		while (samples) {
			float** f;
			unsigned p = fillFloatBuffer(&f, samples);
			if (!p) break;
			samples -= p;
		}
	}

	return newpos != -1;
}

unsigned OggReader::fillFloatBuffer(float*** rpcm, unsigned samples)
{
	while (true) {
		long len = writePos - readPos;
		if (len > 0) {
			ret[0] = pcm[0] + readPos;
			ret[1] = pcm[1] + readPos;
			(*rpcm) = ret;

			if (long(samples) < len) {
				len = samples;
			}
			readPos += len;
			return len;
		}

		if (oggz_read(oggz, 4096) <= 0) return 0;
	}
}

} // namespace openmsx
