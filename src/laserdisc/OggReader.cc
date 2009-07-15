// $Id$

#include "Filename.hh"
#include "LocalFileReference.hh"
#include "OggReader.hh"
#include "MSXException.hh"
#include "yuv2rgb.hh"

#include <cstring>
#include <cstdlib>

#define OGG_SUPPORT

#ifdef OGG_SUPPORT
#include <oggz/oggz.h>
#include <vorbis/codec.h>
#include <theora/theora.h>
#endif

namespace openmsx {

#ifdef OGG_SUPPORT
// POD for C functions
struct Reader {
	OGGZ *oggz;
	int audio_serial;
	int video_serial;

	// video
	int video_remaining_header_packets;
	theora_state video_handle;
	theora_info video_info;
	theora_comment video_comment;
	yuv_buffer frame;
	byte *rawframe;
	int intraframes;

	// audio
	int audio_remaining_header_packets;
	long frames;
	long position;
	float *pcm[2], *ret[2];
	long pcm_size;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
};

static void fill_audio(Reader *reader)
{
	float **pcm;
	long frames;

	for (;(frames = vorbis_synthesis_pcmout(&reader->vd, &pcm));) {
		if (reader->position == reader->frames) {
			reader->position = 0 ;
			reader->frames = 0;
		}

		if (frames + reader->frames > reader->pcm_size) {
			long size = reader->frames + frames + 4096;
			reader->pcm[0] = (float*)realloc(reader->pcm[0],
							size * sizeof(float));
			reader->pcm[1] = (float*)realloc(reader->pcm[1],
							size * sizeof(float));
			reader->pcm_size = size;
		}

		for (long i=0; i<frames; i++) {
			reader->pcm[0][reader->frames + i] = pcm[0][i];
			reader->pcm[1][reader->frames + i] = pcm[1][i];
		}

		vorbis_synthesis_read(&reader->vd, frames);
		reader->frames += frames;
	}
}

static int read_callback(OGGZ * /*oggz*/, ogg_packet *packet, long serial,
							void *userdata)
{
	struct Reader *reader = static_cast<Reader*>(userdata);

	if (reader->audio_serial == -1 && packet->b_o_s && packet->bytes >= 8) {
		if (vorbis_synthesis_idheader(packet)) {
			reader->audio_serial = serial;
                } else if (memcmp(packet->packet, "\200theora", 7) == 0) {
			reader->video_serial = serial;
		}
	}

	if (reader->video_serial == serial) {
		if (theora_packet_isheader(packet)) {
			if (theora_decode_header(&reader->video_info,
					&reader->video_comment, packet) < 0) {
				return 0;
			}

			if (--reader->video_remaining_header_packets == 0) {
				theora_decode_init(&reader->video_handle,
							&reader->video_info);
			}

			return 0;
		} else if (reader->video_remaining_header_packets)
			return 0;

		if (theora_decode_packetin(&reader->video_handle, packet) < 0)
			return 0;

		if (theora_decode_YUVout(&reader->video_handle, &reader->frame) < 0)
			return 0;

		yuv2rgb::convert(reader->rawframe, &reader->frame);

	} else if (reader->audio_serial == serial) {
		if (reader->audio_remaining_header_packets) {
			if (vorbis_synthesis_headerin(&reader->vi, &reader->vc,
								packet) < 0)
				return 0;

			if (!--reader->audio_remaining_header_packets) {
				vorbis_synthesis_init(&reader->vd, &reader->vi);
				vorbis_block_init(&reader->vd, &reader->vb);
			}
		} else {
			if (vorbis_synthesis(&reader->vb, packet) == 0) {
				vorbis_synthesis_blockin(&reader->vd, &reader->vb);
				fill_audio(reader);
			}
		}
	}

	return 0;
}

static int seek_callback(OGGZ * /*oggz*/, ogg_packet *packet, long serial,
							void *userdata)
{
	struct Reader *reader = static_cast<Reader*>(userdata);

	if (reader->video_serial == serial && reader->intraframes == -1) {
		reader->intraframes = packet->granulepos &
			((1 << oggz_get_granuleshift(reader->oggz, serial)) - 1);
	}

	return 0;
}
#endif

OggReader::OggReader(const Filename& filename)
{
#ifdef OGG_SUPPORT
	LocalFileReference file(filename);
	OGGZ *oggz;

	oggz = oggz_open(file.getFilename().c_str(), OGGZ_READ | OGGZ_AUTO);
	if (!oggz) {
		throw MSXException("Failed to open " + file.getFilename());
	}

	reader = new Reader;
	memset(reader, 0, sizeof(Reader));

	reader->audio_serial = -1;
	reader->video_serial = -1;
	reader->video_remaining_header_packets = 3;
	reader->audio_remaining_header_packets = 3;
	reader->rawframe = new byte[640*480*3];
	reader->oggz = oggz;

	oggz_set_read_callback(oggz, -1, read_callback, reader);

	theora_info_init(&reader->video_info);
	theora_comment_init(&reader->video_comment);

	vorbis_info_init(&reader->vi);
	vorbis_comment_init(&reader->vc);

	while ((reader->audio_remaining_header_packets ||
		reader->video_remaining_header_packets) &&
					oggz_read(reader->oggz, 1024) > 0) { /* */ };

	if (reader->video_serial == -1) {
		cleanup();
		throw MSXException("No video track found");
	}

	if (reader->audio_serial == -1) {
		cleanup();
		throw MSXException("No audio track found");
	}

	if (reader->vi.channels != 2) {
		cleanup();
		throw MSXException("Audio must be stereo");
	}

	if (reader->video_info.width != 640 || reader->video_info.height != 480) {
		cleanup();
		throw MSXException("Video must be 640x480");
	}

	if (reader->video_info.fps_numerator != 30000 ||
				reader->video_info.fps_denominator != 1001) {
		cleanup();
		throw MSXException("Video must be 29.97Hz");
	}
#else
	throw MSXException("No support for reading OGG files");
#endif
}

void OggReader::cleanup()
{
#ifdef OGG_SUPPORT
	if (reader->pcm[0])
		free(reader->pcm[0]);

	if (reader->pcm[1])
		free(reader->pcm[1]);

	if (reader->rawframe)
		delete[] reader->rawframe;

	if (reader->audio_remaining_header_packets == 0) {
		vorbis_dsp_clear(&reader->vd);
		vorbis_block_clear(&reader->vb);
	}
	
	if (reader->video_remaining_header_packets == 0)
		theora_clear(&reader->video_handle);

	vorbis_info_clear(&reader->vi);
	vorbis_comment_clear(&reader->vc);

	theora_info_clear(&reader->video_info);
	theora_comment_clear(&reader->video_comment);

	oggz_close(reader->oggz);

	delete reader;
#endif
}

OggReader::~OggReader()
{
	cleanup();
}
bool OggReader::seek(unsigned pos)
{
#ifdef OGG_SUPPORT
	reader->position = 0;
	reader->frames = 0;

	ogg_int64_t newpos = oggz_seek_units(reader->oggz, pos, SEEK_SET);

	if (newpos == -1)
		return false;

	oggz_set_read_callback(reader->oggz, -1, seek_callback, reader);

	reader->intraframes = -1;
	do {
		if (oggz_read(reader->oggz, 4096) <= 0)
			break;
	} while (reader->intraframes == -1);

	oggz_set_read_callback(reader->oggz, -1, read_callback, reader);

	// Seek to the keyframe, two extra for rounding errors
	ogg_int64_t keyframe = pos - (reader->intraframes + 2) * 1001 / 30;
	newpos = oggz_seek_units(reader->oggz, keyframe < 0 ? 0 : keyframe,
								SEEK_SET);

	vorbis_synthesis_restart(&reader->vd);

	if (newpos >= 0 && newpos < pos) {
		float **f;
		unsigned samples = (pos - newpos) * reader->vi.rate / 1000;

		while (samples) {
			unsigned p = fillFloatBuffer(&f, samples);
			if (!p)
				break;

			samples -= p;
		}
	}

	return newpos != -1;
#else
	return false;
#endif
}

byte* OggReader::getFrame()
{
#ifdef OGG_SUPPORT
	return reader->rawframe;
#else
	return NULL;
#endif
}

unsigned OggReader::getSampleRate()
{
#ifdef OGG_SUPPORT
	return reader->vi.rate;
#else
	return 0;
#endif
}

unsigned OggReader::fillFloatBuffer(float ***pcm, unsigned samples)
{
#ifdef OGG_SUPPORT
	long len;

	for (;;) {
		len = reader->frames - reader->position;

		if (len > 0) {
			reader->ret[0] = reader->pcm[0] + reader->position;
			reader->ret[1] = reader->pcm[1] + reader->position;

			(*pcm) = reader->ret;

			if (samples < len)
				len = samples;

			reader->position += len;

			return len;
		}

		if (oggz_read(reader->oggz, 4096) <= 0)
			break;
	}
#endif
	return 0;
}

} // namespace openmsx
