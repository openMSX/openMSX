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
#include "MemoryOps.hh"
#include "stringsp.hh" // for strncasecmp

// TODO
// - Improve error handling
// - When an non-ogg file is passed, the entire file is scanned
// - Clean up this mess!
namespace openmsx {

OggReader::OggReader(const Filename& filename, CliComm& cli_)
	: cli(cli_)
	, file(filename)
{
	audioSerial = -1;
	videoSerial = -1;
	skeletonSerial = -1;
	videoHeaders = 3;
	audioHeaders = 3;
	keyFrame = -1;
	currentSample = 0;
	currentFrame = 1;
	vorbisPos = 0;

	th_info_init(&video_info);
	th_comment_init(&video_comment);
	video_state = NULL;
	video_setup_info = NULL;

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	frameList.clear();
	recycleFrameList.clear();

	audioList.clear();
	recycleAudioList.clear();

	chapters.clear();
	stopFrames.clear();

	ogg_sync_init(&sync);

	state = PLAYING;
	currentOffset = 0;
	totalBytes = file.getSize();

	ogg_page page;

	while ((audioHeaders || videoHeaders) && nextPage(&page)) {
		int serial = ogg_page_serialno(&page);

		if (serial == audioSerial) {
			vorbisHeaderPage(&page);
			continue;
		} else if (serial == videoSerial) {
			theoraHeaderPage(&page);
			continue;
		} else if (serial == skeletonSerial) {
			continue;
		}

		if (!ogg_page_bos(&page)) {
			if (videoSerial == -1) {
				cleanup();
				throw MSXException("No video track found");
			}

			if (audioSerial == -1) {
				cleanup();
				throw MSXException("No audio track found");
			}

			/* This should be unreachable, right? */
			continue;
		}

		ogg_stream_state stream;
		ogg_packet packet;

		ogg_stream_init(&stream, serial);
		ogg_stream_pagein(&stream, &page);
		if (ogg_stream_packetout(&stream, &packet) <= 0) {
			ogg_stream_clear(&stream);
			cleanup();
			throw MSXException("Invalid header");
		}

		if (packet.bytes < 8) {
			ogg_stream_clear(&stream);
			cleanup();
			throw MSXException("Header to small");
		}

		if (memcmp(packet.packet, "\x01vorbis", 7) == 0) {
			if (audioSerial != -1) {
				ogg_stream_clear(&stream);
				cleanup();
				throw MSXException("Duplicate audio stream");
			}

			audioSerial = serial;
			ogg_stream_init(&vorbisStream, serial);

			vorbisHeaderPage(&page);
		} 
		else if (memcmp(packet.packet, "\x80theora", 7) == 0) {
			if (videoSerial != -1) {
				ogg_stream_clear(&stream);
				cleanup();
				throw MSXException("Duplicate video stream");
			}

			if (packet.bytes < 42) {
				ogg_stream_clear(&stream);
				cleanup();
				throw MSXException("Theora header to small");
			}

			videoSerial = serial;
			ogg_stream_init(&theoraStream, serial);

			granuleShift = ((packet.packet[40] & 3) << 3) |
					((packet.packet[41] & 0xe0) >> 5);

			theoraHeaderPage(&page);
		}
		else if (memcmp(packet.packet, "fishead", 8) == 0) {
			skeletonSerial = serial;
		}
		else if (memcmp(packet.packet, "BBCD", 4) == 0) {
			ogg_stream_clear(&stream);
			cleanup();
			throw MSXException("DIRAC not supported");
		}
		else if (memcmp(packet.packet, "\177FLAC", 5) == 0) {
			ogg_stream_clear(&stream);
			cleanup();
			throw MSXException("FLAC not supported");
		}
		else  {
			ogg_stream_clear(&stream);
			cleanup();
			throw MSXException("Unknown stream in ogg file");
		}

		ogg_stream_clear(&stream);
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

	if (video_info.frame_width != 640 || video_info.frame_height != 480) {
		cleanup();
		throw MSXException("Video must be size 640x480");
	}

	if (video_info.fps_numerator != 30000 ||
	    video_info.fps_denominator != 1001) {
		cleanup();
		throw MSXException("Video must be 29.97Hz");
	}

	// FIXME: Support YUV444 before release
	// It would be much better to use YUV444, however the existing
	// captures are in YUV420 format. yuv2rgb will have to be updated
	// too.
	if (video_info.pixel_fmt != TH_PF_420) {
		cleanup();
		throw MSXException("Video must be YUV420");
	}
}

void OggReader::cleanup()
{
	if (audioHeaders == 0) {
		vorbis_dsp_clear(&vd);
		vorbis_block_clear(&vb);
	}

	if (video_state != NULL) {
		th_decode_free(video_state);
	}

	if (video_setup_info != NULL) {
		th_setup_free(video_setup_info);
	}

	while (!frameList.empty()) {
		Frame *frame = frameList.front();
		MemoryOps::freeAligned(frame->buffer[0].data);
		MemoryOps::freeAligned(frame->buffer[1].data);
		MemoryOps::freeAligned(frame->buffer[2].data);
		delete frame;
		frameList.pop_front();
	}

	while (!recycleFrameList.empty()) {
		Frame *frame = recycleFrameList.front();
		MemoryOps::freeAligned(frame->buffer[0].data);
		MemoryOps::freeAligned(frame->buffer[1].data);
		MemoryOps::freeAligned(frame->buffer[2].data);
		delete frame;
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
	if (audioSerial != -1) {
		ogg_stream_clear(&vorbisStream);
	}

	th_info_clear(&video_info);
	th_comment_clear(&video_comment);
	if (videoSerial != -1) {
		ogg_stream_clear(&theoraStream);
	}

	ogg_sync_clear(&sync);
}

OggReader::~OggReader()
{
	cleanup();
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


void OggReader::vorbisHeaderPage(ogg_page* page)
{
	ogg_stream_pagein(&vorbisStream, page);

	while (true) {
		ogg_packet packet;

		int res	= ogg_stream_packetout(&vorbisStream, &packet);

		if (res < 0) {
			throw MSXException("error in vorbis stream");
		}

		if (res == 0) break;

		if (audioHeaders == 0) {
			readVorbis(&packet);
		}

		if (packet.packetno <= 2) {
			if (vorbis_synthesis_headerin(&vi, &vc, &packet) < 0) {
				throw MSXException("invalid vorbis header");
			}
			audioHeaders--;
		}

		if (packet.packetno == 2) {
			vorbis_synthesis_init(&vd, &vi) ;
			vorbis_block_init(&vd, &vb);
		}
	}
}

void OggReader::theoraHeaderPage(ogg_page* page)
{
	ogg_stream_pagein(&theoraStream, page);

	while (true) {
		ogg_packet packet;

		int res	= ogg_stream_packetout(&theoraStream, &packet);

		if (res < 0) {
			throw MSXException("error in vorbis stream");
		}

		if (res == 0) break;

		if (videoHeaders == 0) {
			readTheora(&packet);
		}

		if (packet.packetno <= 2) {
			res = th_decode_headerin(&video_info, &video_comment,
						&video_setup_info, &packet);
			if (res <= 0) {
				throw MSXException("invalid theora header");
			}
			videoHeaders--;
		}

		if (packet.packetno == 2) {
			video_state = th_decode_alloc(&video_info, 
							video_setup_info);
			readMetadata();
		}
	}
}

void OggReader::readVorbis(ogg_packet* packet)
{
	// deal with header packets
	if (unlikely(packet->packetno <= 2)) {
		return;
	}

	if (state == FIND_LAST) {
		if (packet->granulepos != -1) {
			if (currentSample == AudioFragment::UNKNOWN_POS ||
					packet->granulepos > currentSample) {
				currentSample = packet->granulepos;
			}
		}

		return;
	} else if (state == FIND_FIRST) {
		if (packet->granulepos != -1 &&
				currentSample == AudioFragment::UNKNOWN_POS) {
			currentSample = packet->granulepos;
		}

		return;
	} else if (state == FIND_KEYFRAME) {
		// Not relevant for vorbis
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
	int key, intra;

	if (packet->granulepos == -1) {
		return -1;
	}

	intra = packet->granulepos & ((1 << granuleShift) - 1);
	key = packet->granulepos >> granuleShift;

	return key + intra;
}

void OggReader::readMetadata()
{
	char *metadata = NULL, *p;

	for (int i=0; i < video_comment.comments; i++) {
		if (video_comment.user_comments[i] &&
			video_comment.comment_lengths[i] >=
							int(sizeof("location="))
			&& !strncasecmp(video_comment.user_comments[i],
					"location=", sizeof("location=") - 1)) {
			// FIXME: already null terminated
			size_t len = video_comment.comment_lengths[i] - 
						sizeof("location=");
			metadata = new char[len + 1];
			memcpy(metadata, video_comment.user_comments[i] +
						sizeof("location=") - 1, len);
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
	if (unlikely(packet->packetno <= 2)) {
		return;
	}

	int frameno = frameNo(packet);

	if (state != PLAYING && frameno == -1) {
		return;
	}

	if (state == FIND_LAST) {
		if (currentFrame == -1 || currentFrame < frameno) {
			currentFrame = frameno;
		}

		return;
	} else if (state == FIND_FIRST) {
		if (currentFrame == -1 || currentFrame > frameno) {
			currentFrame = frameno;
		}

		return;
	} else if (state == FIND_KEYFRAME) {
		if (currentFrame == frameno) {
			keyFrame = packet->granulepos >> granuleShift;
		}
		return;
	}

	if (keyFrame != -1 && frameno != -1 && frameno < keyFrame) {
		// We're reading before the keyframe, discard
		return;
	}

	keyFrame = -1;

	if (th_decode_packetin(video_state, packet, NULL) != 0) {
		return;
	}

	th_ycbcr_buffer yuv;

	if (th_decode_ycbcr_out(video_state, yuv) != 0) {
		return;
	}

	//FIXME: assert(frameno != -1);

	if (frameno < currentFrame) {
		return;
	}

	currentFrame = frameno + 1;

	Frame* frame;
	int y_size = yuv[0].height * yuv[0].stride;
	int uv_size = yuv[1].height * yuv[1].stride;

	if (recycleFrameList.empty()) {
		frame = new Frame;
		frame->buffer[0] = yuv[0];
		frame->buffer[0].data = static_cast<unsigned char*>(
					MemoryOps::mallocAligned(16, y_size));
		frame->buffer[1] = yuv[1];
		frame->buffer[1].data = static_cast<unsigned char*>(
					MemoryOps::mallocAligned(16, uv_size));
		frame->buffer[2] = yuv[2];
		frame->buffer[2].data = static_cast<unsigned char*>(
					MemoryOps::mallocAligned(16, uv_size));
	} else {
		frame = recycleFrameList.front();
		recycleFrameList.pop_front();
	}

	memcpy(frame->buffer[0].data, yuv[0].data, y_size);
	memcpy(frame->buffer[1].data, yuv[1].data, uv_size);
	memcpy(frame->buffer[2].data, yuv[2].data, uv_size);
	frame->no = frameno;

	frameList.push_back(frame);
}

void OggReader::getFrame(RawFrame& rawFrame, int frameno)
{
	Frame* frame;

	// Note that when frames are unchanged they will simply not be
	// present in the theora stream. This means that with frames
	// 8,9,10,13,14 in the list, 11 and 12 must be drawn as 10.

	while (true) {
		// Remove unneeded frames
		while (frameList.size() >= 2 && frameList[1]->no <= frameno) {
			recycleFrameList.push_back(frameList[0]);
			frameList.pop_front();
		}

		if (frameList.size() >= 2 && (frameList[0]->no == frameno ||
					frameList[1]->no >= frameno)) {
			frame = frameList[0];
			break;
		}

		// ..add read some new ones
		if (!nextPacket()) {
			return;
		}
	}

	yuv2rgb::convert(frame->buffer, rawFrame);
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
		if (!nextPacket()) {
			return NULL;
		}
	}

	AudioFragments::iterator it = audioList.begin();

	while (true) {
		AudioFragment* audio = *it;

		if (audio->position + audio->length + getSampleRate() <= sample) {
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
				if (!nextPacket()) {
					return NULL;
				}
			}

			// reset the iterator to not point to the end
			it = audioList.begin();
		}
	}

	return NULL;
}

bool OggReader::nextPacket()
{
	int ret;
	ogg_packet packet;
	ogg_page page;

	while (true) {
		ret = ogg_stream_packetout(&vorbisStream, &packet);
		if (ret == 1) {
			readVorbis(&packet);
			return true;
		} else if (ret == -1) {
			// recoverable error
			continue;
		}

		ret = ogg_stream_packetout(&theoraStream, &packet);
		if (ret == 1) {
			readTheora(&packet);
			return true;
		} else if (ret == -1) {
			// recoverable error
			continue;
		}

		if (!nextPage(&page)) {
			return false;
		}

		int serial = ogg_page_serialno(&page);

		if (serial == audioSerial) {
			if (ogg_stream_pagein(&vorbisStream, &page)) {
				cli.printWarning("Failed to submit vorbis page");
			}
		} else if (serial == videoSerial) {
			if (ogg_stream_pagein(&theoraStream, &page)) {
				cli.printWarning("Failed to submit theora page");
			}
		} else if (serial != skeletonSerial) {
			cli.printWarning("Unexpected stream with serial " +
				StringOp::toString(serial) + " in ogg file");
		}
	}
}

#define CHUNK 4096

bool OggReader::nextPage(ogg_page* page)
{
	int ret;

	while ((ret = ogg_sync_pageseek(&sync, page)) <= 0) {
		if (ret < 0) {
			//throw MSXException("Invalid Ogg file");
		}

		unsigned chunk;

		if (totalBytes - currentOffset >= CHUNK) {
			chunk = CHUNK;
		} else if (currentOffset < totalBytes) {
			chunk = totalBytes - currentOffset;
		} else {
			return false;
		}
	
		char *buffer = ogg_sync_buffer(&sync, chunk);
		file.read(buffer, chunk);
		currentOffset += chunk;

		if (ogg_sync_wrote(&sync, chunk) == -1) {
			cli.printWarning("Internal error: ogg_sync_wrote failed");
		}
	}

	return true;
}
#undef CHUNK

// Defined to be a power-of-two such that the arthmetic can be done faster.
// Note that the sample-number is in the range of: 1..(44100*60*60)
#define SHIFT 0x20000000ull

unsigned OggReader::binarySearch(int frame, unsigned sample,
		unsigned maxOffset, unsigned maxSamples, unsigned maxFrames)
{
	uint64 offsetA = 0, offsetB = maxOffset;
	uint64 sampleA = 0, sampleB = maxSamples;
	uint64 frameA = 1, frameB = maxFrames;
	unsigned offset;

	for (;;) {
		uint64 ratio, frameOffset, sampleOffset;

		ratio = (frame - frameA) * SHIFT / (frameB - frameA);
		if (ratio < 5) {
			return offsetA;
		}

		frameOffset = ratio * (offsetB - offsetA) / SHIFT + offsetA;
		ratio = (sample - sampleA) * SHIFT / (sampleB - sampleA);
		if (ratio < 5) {
			return offsetA;
		}
		sampleOffset = ratio * (offsetB - offsetA) / SHIFT + offsetA;
		offset = sampleOffset < frameOffset ? 
					sampleOffset : frameOffset;
	
		file.seek(offset);
		currentOffset = offset;
		ogg_sync_reset(&sync);
		currentFrame = -1;
		currentSample = AudioFragment::UNKNOWN_POS;
		state = FIND_FIRST;

		while ((currentFrame == -1 || 
			 currentSample == AudioFragment::UNKNOWN_POS) && 
			nextPacket()) {
			// continue reading
		}

		state = PLAYING;

		if (currentSample > sample || currentFrame > frame) {
			offsetB = offset;
			sampleB = currentSample;
			frameB = currentFrame;
		} else if (currentSample + getSampleRate() < sample &&
				currentFrame + 64 < frame) {
			offsetA = offset;
			sampleA = currentSample;
			frameA = currentFrame;
		} else {
			break;
		}
	}

	return offset;
}
#undef SHIFT

#define STEP 32*1024

unsigned OggReader::guessSeek(int frame, unsigned sample)
{
	// first calculate total length in bytes, samples and frames
	unsigned offset = file.getSize();
	
	totalBytes = offset;

	while (offset > 0) {
		if (offset > STEP) {
			offset -= STEP;
		} else {
			offset= 0;
		}

		file.seek(offset);
		currentOffset = offset;
		ogg_sync_reset(&sync);
		currentFrame = -1;
		currentSample = AudioFragment::UNKNOWN_POS;
		state = FIND_LAST;

		while (nextPacket()) {
			// continue reading
		}

		state = PLAYING;

		if (currentFrame != -1 && currentSample != 
					AudioFragment::UNKNOWN_POS) {
			break;
		}
	}

	if (sample < getSampleRate() || frame < 30) {
		keyFrame = 1;
		return 0;
	}

	unsigned maxOffset = offset;
	unsigned maxSamples = currentSample;
	unsigned maxFrames = currentFrame;

	if (sample > maxSamples || unsigned(frame) > maxFrames) {
		sample = maxSamples;
		frame = maxFrames;
	}

	offset = binarySearch(frame, sample, maxOffset, maxSamples, maxFrames);

	// Find key frame
	file.seek(offset);
	currentOffset = offset;
	ogg_sync_reset(&sync);
	currentFrame = frame;
	keyFrame = -1;
	state = FIND_KEYFRAME;

	while (keyFrame == -1 && nextPacket()) {
		// continue reading
	}

	state = PLAYING;

	if (keyFrame == -1 || frame == keyFrame) {
		return offset;
	}

	return binarySearch(keyFrame, sample, maxOffset, maxSamples, maxFrames);
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
		AudioFragment* audio = audioList.front();
		audio->length = 0;
		audio->position = 0;
		recycleAudioList.push_back(audio);
		audioList.pop_front();
	}

	currentOffset = guessSeek(frame, samples);		
	file.seek(currentOffset);

	ogg_sync_reset(&sync);

	vorbisPos = AudioFragment::UNKNOWN_POS;
	currentFrame = frame;
	currentSample = samples;

	vorbis_synthesis_restart(&vd);

	return true;
}

} // namespace openmsx
