// $Id$

#include "OggReader.hh"
#include "Filename.hh"
#include "MSXException.hh"
#include "yuv2rgb.hh"
#include "likely.hh"
#include "Clock.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "RawFrame.hh"

#include <cstring>

namespace openmsx {

OggReader::OggReader(const std::string& filename, CliComm& cli_)
	: cli(cli_)
{
	LocalFileReference file(filename);
	oggz = oggz_open(file.getFilename().c_str(), OGGZ_READ | OGGZ_AUTO);
	if (!oggz) {
		throw MSXException("Failed to open " + filename);
	}

	audioSerial = -1;
	videoSerial = -1;
	videoHeaders = 3;
	audioHeaders = 3;
	keyFrame = -1;
	currentSample = 0;
	currentFrame = 1;
	vorbisPos = 0;

	oggz_set_read_callback(oggz, -1, readCallback, this);

	theora_info_init(&video_info);
	theora_comment_init(&video_comment);

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	frameList.clear();
	recycleFrameList.clear();

	audioList.clear();
	recycleAudioList.clear();

	chapters.clear();
	stopFrames.clear();

	while (audioHeaders || videoHeaders) {
		if (oggz_read(oggz, 1024) <= 0) {
			break;
		}
	}

	if (videoSerial == -1) {
		cleanup();
		throw MSXException("No video track found");
	}

	if (audioSerial == -1) {
		cleanup();
		throw MSXException("No audio track found");
	}

	if (vi.channels != 2) {
		cleanup();
		throw MSXException("Audio must be stereo");
	}

	if (video_info.width != 640 || video_info.height != 480) {
		cleanup();
		throw MSXException("Video must be size 640x480");
	}

	if (video_info.fps_numerator != 30000 ||
	    video_info.fps_denominator != 1001) {
		cleanup();
		throw MSXException("Video must be 29.97Hz");
	}
}

void OggReader::cleanup()
{
	if (audioHeaders == 0) {
		vorbis_dsp_clear(&vd);
		vorbis_block_clear(&vb);
	}

	if (videoHeaders == 0) {
		theora_clear(&video_handle);
	}

	while (!frameList.empty()) {
		yuv_buffer *buffer = frameList.front();
		delete[] buffer->y;
		delete[] buffer->u;
		delete[] buffer->v;
		delete buffer;
		frameList.pop_front();
	}

	while (!recycleFrameList.empty()) {
		yuv_buffer *buffer = recycleFrameList.front();
		delete[] buffer->y;
		delete[] buffer->u;
		delete[] buffer->v;
		delete buffer;
		recycleFrameList.pop_front();
	}

	while (!audioList.empty()) {
		delete audioList.front();
		audioList.pop_front();
	}

	while (!recycleAudioList.empty()) {
		delete recycleAudioList.front();
		recycleAudioList.pop_front();
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

int OggReader::readCallback(OGGZ* /*oggz*/, ogg_packet* packet, long serial,
								void* userdata)
{
	OggReader* reader = static_cast<OggReader*>(userdata);

	if (unlikely(packet->b_o_s && packet->bytes >= 8)) {
		if (memcmp(packet->packet, "\001vorbis", 7) == 0) {
			reader->audioSerial = serial;
		} else if (memcmp(packet->packet, "\200theora", 7) == 0) {
			reader->videoSerial = serial;
		}
	}

	if (reader->videoSerial == serial) {
		reader->readTheora(packet);
	} else if (reader->audioSerial == serial) {
		reader->readVorbis(packet);
	}

	return 0;
}

int OggReader::seekCallback(OGGZ* /*oggz*/, ogg_packet* packet, long serial,
								void* userdata)
{
	OggReader* reader = static_cast<OggReader*>(userdata);
	int shift = oggz_get_granuleshift(reader->oggz, serial);

	if (reader->videoSerial == serial && reader->keyFrame == -1 &&
					!theora_packet_isheader(packet)) {

		int frame = reader->frameNo(packet);

		if (reader->currentFrame == frame) {
			reader->keyFrame = packet->granulepos >> shift;
		} else if (frame > reader->currentFrame) {
			// gone past, seek from beginning
			reader->cli.printWarning("Keyframe " +
				StringOp::toString(reader->currentFrame) +
				" not found in video, replaying from start. "
				"Seen frame " + StringOp::toString(frame));

			reader->keyFrame = 0;
		}
	}

	return 0;
}

/** Vorbis only records the ogg position (in no. of samples) once per ogg
 * page. After seeking we have already decoded some audio before we encounter
 * the exact position we are at. Fixup the positions and discard any unwanted
 * audio. This function expects vorbisPos to be set correctly.
 */
void OggReader::vorbisFoundPosition()
{
	unsigned last = vorbisPos;

	for (AudioFragments::reverse_iterator it = audioList.rbegin();
						it != audioList.rend(); ++it) {
		last -= (*it)->length;
		(*it)->position = last;
	}

	// last is now the first vorbis audio decoded
	if (last > currentSample) {
		cli.printWarning("missing part of audio stream");
	}

	if (vorbisPos > currentSample) {
		currentSample = vorbisPos;
	}
}

void OggReader::readVorbis(ogg_packet* packet)
{
	// deal with header packets
	if (unlikely(audioHeaders)) {
		if (vorbis_synthesis_headerin(&vi, &vc, packet) < 0) {
			return;
		}

		if (!--audioHeaders) {
			vorbis_synthesis_init(&vd, &vi);
			vorbis_block_init(&vd, &vb);
		}

		return;
	}

	// generate pcm
	if (vorbis_synthesis(&vb, packet) != 0) {
		return;
	}

	vorbis_synthesis_blockin(&vd, &vb);

	float** pcm;
	long decoded = vorbis_synthesis_pcmout(&vd, &pcm);
	long pos = 0;

	while (pos < decoded)  {
		bool last = false;
		AudioFragment* audio;

		// Find memory to copy PCM into
		if (recycleAudioList.empty()) {
			audio = new AudioFragment;
			audio->length = 0;
			audio->position = vorbisPos;
			recycleAudioList.push_front(audio);
		} else {
			audio = recycleAudioList.front();
			if (audio->length == 0) {
				audio->position = vorbisPos;
			}
		}

		// Copy PCM
		long len = std::min(decoded - pos,
			long(AudioFragment::MAX_SAMPLES - audio->length));

		memcpy(audio->pcm[0] + audio->length, pcm[0] + pos,
						len * sizeof(float));
		memcpy(audio->pcm[1] + audio->length, pcm[1] + pos,
						len * sizeof(float));

		audio->length += len;
		pos += len;

		// Last packet or found position after seeking?
		if (decoded == pos && (packet->e_o_s ||
				(vorbisPos == AudioFragment::UNKNOWN_POS &&
						packet->granulepos != -1))) {
			last = true;
		}

		if (vorbisPos != AudioFragment::UNKNOWN_POS) {
			vorbisPos += len;
			currentSample += len;
		}

		if (audio->length == AudioFragment::MAX_SAMPLES || last) {
			recycleAudioList.pop_front();
			audioList.push_back(audio);
		}
	}

	// The granulepos is the no. of samples since the begining of the
	// stream. Only once per ogg page is this populated.
	if (packet->granulepos != -1) {
		if (vorbisPos == AudioFragment::UNKNOWN_POS) {
			vorbisPos = packet->granulepos;
			vorbisFoundPosition();
		} else {
			if (vorbisPos != packet->granulepos) {
				cli.printWarning("vorbis audio out of sync, "
					"expected " +
					StringOp::toString(vorbisPos) +
					", got " +
					StringOp::toString(packet->granulepos));

				vorbisPos = packet->granulepos;
			}
		}
	}

	// done with PCM data
	vorbis_synthesis_read(&vd, decoded);
}

int OggReader::frameNo(ogg_packet* packet)
{
	int key, intra, shift;

	shift = oggz_get_granuleshift(oggz, videoSerial);

	intra = packet->granulepos & ((1 << shift) - 1);
	key = packet->granulepos >> shift;

	return key + intra;
}

void OggReader::readMetadata()
{
	char *metadata = NULL, *p;

	for (int i=0; i < video_comment.comments; i++) {
		if (video_comment.user_comments[i] &&
			video_comment.comment_lengths[i] >=
							int(sizeof("location"))
			&& !strcasecmp(video_comment.user_comments[i],
								"location")) {
			// ensure null termination
			size_t len = video_comment.comment_lengths[i] - 
							sizeof("location");
			metadata = new char[len + 1];
			memcpy(metadata, video_comment.user_comments[i] +
						sizeof("location"), len);
			metadata[len] = 0;
			break;
		}
	}

	if (!metadata) {
		return;
	}

	p = metadata;

	// Maybe there is a better way of doing this parsing in C++
	while (p) {
		if (strncmp(p, "chapter: ", 8) == 0) {
			p += 8;
			int chapter = atoi(p);
			p = strchr(p, ',');
			if (!p) {
				break;
			}
			p++;
			int frame = atoi(p);
			if (frame) {
				chapters[chapter] = frame;
			}

		} else if (strncmp(p, "stop: ", 6) == 0) {
			int stopframe = atoi(p + 6);
			if (stopframe) {
				stopFrames[stopframe] =  1;
			}
		}
		p = strchr(p, '\n');
		if (p) p++;
	}

	delete[] metadata;
}

void OggReader::readTheora(ogg_packet* packet)
{
	if (unlikely(theora_packet_isheader(packet))) {
		if (!videoHeaders) {
			// header already read; when playing from the
			// beginning after seeking to the beginning, header
			// packets might be seen again.
			return;
		}

		if (theora_decode_header(&video_info, &video_comment,
							 packet) < 0) {
			return;
		}

		if (--videoHeaders == 0) {
			theora_decode_init(&video_handle, &video_info);
			readMetadata();
		}

		return;
	} else if (videoHeaders) {
		cli.printWarning("Missing header packets for video stream");
		return;
	}

	int frameno = frameNo(packet);

	if (keyFrame != -1 && frameno < keyFrame) {
		// We're reading before the keyframe, discard
		return;
	}

	keyFrame = -1;

	if (theora_decode_packetin(&video_handle, packet) < 0) {
		return;
	}

	yuv_buffer yuv_theora;

	if (theora_decode_YUVout(&video_handle, &yuv_theora) < 0) {
		return;
	}

	if (frameno < currentFrame) {
		return;
	}

	if (frameno != currentFrame) {
		cli.printWarning("Unexpected theora frame " +
			StringOp::toString(frameno) + ", expected " +
			StringOp::toString(currentFrame));

		currentFrame = frameno;
	}

	yuv_buffer* yuvcopy;
	int y_size = yuv_theora.y_height * yuv_theora.y_stride;
	int uv_size = yuv_theora.uv_height * yuv_theora.uv_stride;

	if (recycleFrameList.empty()) {
		yuvcopy = new yuv_buffer;
		yuvcopy->y = new byte[y_size];
		yuvcopy->u = new byte[uv_size];
		yuvcopy->v = new byte[uv_size];

		yuvcopy->y_width = yuv_theora.y_width;
		yuvcopy->y_height = yuv_theora.y_height;
		yuvcopy->y_stride = yuv_theora.y_stride;
		yuvcopy->uv_width = yuv_theora.uv_width;
		yuvcopy->uv_height = yuv_theora.uv_height;
		yuvcopy->uv_stride = yuv_theora.uv_stride;
	} else {
		yuvcopy = recycleFrameList.front();
		recycleFrameList.pop_front();
	}

	memcpy(yuvcopy->y, yuv_theora.y, y_size);
	memcpy(yuvcopy->u, yuv_theora.u, uv_size);
	memcpy(yuvcopy->v, yuv_theora.v, uv_size);

	frameList.push_back(yuvcopy);

	currentFrame++;
}

void OggReader::getFrame(RawFrame& frame)
{
	while (frameList.empty()) {
		if (oggz_read(oggz, 4096) <= 0) {
			return;
		}
	}

	yuv_buffer* current = frameList.front();

	yuv2rgb::convert(*current, frame);

	frameList.pop_front();

	recycleFrameList.push_back(current);

	return;
}

void OggReader::returnAudio(AudioFragment* audio)
{
	audio->length = 0;
	audio->position = 0;

	recycleAudioList.push_back(audio);
}

AudioFragment* OggReader::getAudio(unsigned sample)
{
	// Read while position is unknown
	while (audioList.empty() || audioList.front()->position ==
						AudioFragment::UNKNOWN_POS) {
		if (oggz_read(oggz, 4096) <= 0) {
			return NULL;
		}
	}

	AudioFragments::iterator it = audioList.begin();

	while (true) {
		AudioFragment* audio = *it;

		if (audio->position + audio->length + vi.rate <= sample) {
			// Dispose if this, more than 1 second old
			returnAudio(*it);
			it = audioList.erase(it);
		} else if (audio->position + audio->length <= sample) {
			it++;
		} else {
			if (audio->position <= sample) {
				return audio;
			} else {
				// gone too far?
				return NULL;
			}
		}

		// read more if we're at the end of the list
		if (it == audioList.end()) {
			unsigned size = audioList.size();

			while (size == audioList.size()) {
				if (oggz_read(oggz, 4096) <= 0) {
					return NULL;
				}
			}

			// reset the iterator to not point to the end
			it = audioList.begin();
		}
	}

	return NULL;
}

bool OggReader::seek(int frame, int samples)
{
	// Remove all queued frames
	while (!frameList.empty()) {
		recycleFrameList.push_back(frameList.front());
		frameList.pop_front();
	}

	// Remove all queued audio
	if (!recycleAudioList.empty()) {
		recycleAudioList.front()->length = 0;
	}

	while (!audioList.empty()) {
		AudioFragment *audio = audioList.front();
		audio->length = 0;
		audio->position = 0;
		recycleAudioList.push_back(audio);
		audioList.pop_front();
	}

	currentFrame = frame;
	currentSample = samples;
	vorbisPos = AudioFragment::UNKNOWN_POS;
	ogg_int64_t pos = (frame - 3) * 1001ll / 30ll;

	if (pos < 0) pos = 0;

	// Find key frame for video
	ogg_int64_t newpos = oggz_seek_units(oggz, pos, SEEK_SET);

	if (newpos == -1) {
		return false;
	}

	oggz_set_read_callback(oggz, -1, seekCallback, this);

	keyFrame = -1;
	do {
		if (oggz_read(oggz, 4096) <= 0) {
			break;
		}
	} while (keyFrame == -1);

	oggz_set_read_callback(oggz, -1, readCallback, this);

	// Found keyframe, now calculate position for audio and video and
	// seek to the earliest position.
	ogg_int64_t keyframepos = (keyFrame - 3) * 1001ll / 30ll;
	ogg_int64_t samplepos = samples * 1000ll / vi.rate;

	pos = std::min(keyframepos, samplepos);

	if (pos < 0) pos = 0;

	newpos = oggz_seek_units(oggz, pos, SEEK_SET);

	vorbis_synthesis_restart(&vd);

	return newpos != -1;
}

} // namespace openmsx
