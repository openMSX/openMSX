#include "OggReader.hh"
#include "MSXException.hh"
#include "yuv2rgb.hh"
#include "CliComm.hh"
#include "MemoryOps.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stringsp.hh" // for strncasecmp
#include "view.hh"
#include "xrange.hh"
#include <cstdlib> // for atoi
#include <cctype> // for isspace
#include <memory>

// TODO
// - Improve error handling
// - When an non-ogg file is passed, the entire file is scanned
// - Clean up this mess!
namespace openmsx {

Frame::Frame(const th_ycbcr_buffer& yuv)
{
	unsigned y_size  = yuv[0].height * yuv[0].stride;
	unsigned uv_size = yuv[1].height * yuv[1].stride;

	buffer[0] = yuv[0];
	buffer[0].data = static_cast<unsigned char*>(
			MemoryOps::mallocAligned(16, y_size));
	buffer[1] = yuv[1];
	buffer[1].data = static_cast<unsigned char*>(
			MemoryOps::mallocAligned(16, uv_size));
	buffer[2] = yuv[2];
	buffer[2].data = static_cast<unsigned char*>(
			MemoryOps::mallocAligned(16, uv_size));
}

Frame::~Frame()
{
	MemoryOps::freeAligned(buffer[0].data);
	MemoryOps::freeAligned(buffer[1].data);
	MemoryOps::freeAligned(buffer[2].data);
}


OggReader::OggReader(const Filename& filename, CliComm& cli_)
	: cli(cli_)
	, file(filename)
	, fileSize(file.getSize())
{
	th_info ti;
	th_comment tc;
	th_setup_info* tsi = nullptr;

	th_info_init(&ti);
	th_comment_init(&tc);

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	ogg_sync_init(&sync);

	ogg_page page;

	try {
		while ((audioHeaders || !theora) && nextPage(&page)) {
			int serial = ogg_page_serialno(&page);

			if (serial == audioSerial) {
				vorbisHeaderPage(&page);
				continue;
			} else if (serial == videoSerial) {
				theoraHeaderPage(&page, ti, tc, tsi);
				continue;
			} else if (serial == skeletonSerial) {
				continue;
			}

			if (!ogg_page_bos(&page)) {
				if (videoSerial == -1) {
					throw MSXException("No video track found");
				}

				if (audioSerial == -1) {
					throw MSXException("No audio track found");
				}

				// This should be unreachable, right?
				continue;
			}

			ogg_stream_state stream;
			ogg_packet packet;

			ogg_stream_init(&stream, serial);
			ogg_stream_pagein(&stream, &page);
			if (ogg_stream_packetout(&stream, &packet) <= 0) {
				ogg_stream_clear(&stream);
				throw MSXException("Invalid header");
			}

			if (packet.bytes < 8) {
				ogg_stream_clear(&stream);
				throw MSXException("Header to small");
			}

			if (ranges::equal(std::span{packet.packet, 7}, std::array<uint8_t, 7>{1, 'v', 'o', 'r', 'b', 'i', 's'})) {
				if (audioSerial != -1) {
					ogg_stream_clear(&stream);
					throw MSXException("Duplicate audio stream");
				}

				audioSerial = serial;
				ogg_stream_init(&vorbisStream, serial);

				vorbisHeaderPage(&page);

			} else if (ranges::equal(std::span{packet.packet, 7}, std::array<uint8_t, 7>{128, 't', 'h', 'e', 'o', 'r', 'a'})) {
				if (videoSerial != -1) {
					ogg_stream_clear(&stream);
					throw MSXException("Duplicate video stream");
				}

				videoSerial = serial;
				ogg_stream_init(&theoraStream, serial);

				theoraHeaderPage(&page, ti, tc, tsi);

			} else if (ranges::equal(std::span{packet.packet, 8}, std::array<uint8_t, 8>{'f', 'i', 's', 'h', 'e', 'a', 'd', 0})) {
				skeletonSerial = serial;

			} else if (ranges::equal(std::span{packet.packet, 4}, std::array<uint8_t, 4>{'B', 'B', 'C', 'D'})) {
				ogg_stream_clear(&stream);
				throw MSXException("DIRAC not supported");

			} else if (ranges::equal(std::span{packet.packet, 5}, std::array<uint8_t, 5>{127, 'F', 'L', 'A', 'C'})) {
				ogg_stream_clear(&stream);
				throw MSXException("FLAC not supported");

			} else	{
				ogg_stream_clear(&stream);
				throw MSXException("Unknown stream in ogg file");
			}

			ogg_stream_clear(&stream);
		}

		if (videoSerial == -1) {
			throw MSXException("No video track found");
		}

		if (audioSerial == -1) {
			throw MSXException("No audio track found");
		}

		if (vi.channels != 2) {
			throw MSXException("Audio must be stereo");
		}

		if (ti.frame_width != 640 || ti.frame_height != 480) {
			throw MSXException("Video must be size 640x480");
		}

		if (ti.fps_numerator == 30000 && ti.fps_denominator == 1001) {
			frameRate = 30;
		} else if (ti.fps_numerator == 60000 && ti.fps_denominator == 1001) {
			frameRate = 60;
		} else {
			throw MSXException("Video frame rate must be 59.94Hz or 29.97Hz");
		}

		// TODO: Support YUV444 before release
		// It would be much better to use YUV444, however the existing
		// captures are in YUV420 format. yuv2rgb will have to be
		// updated too.
		if (ti.pixel_fmt != TH_PF_420) {
			throw MSXException("Video must be YUV420");
		}
	} catch (MSXException&) {
		th_setup_free(tsi);
		th_info_clear(&ti);
		th_comment_clear(&tc);
		cleanup();
		throw;
	}

	th_setup_free(tsi);
	th_info_clear(&ti);
	th_comment_clear(&tc);
}

void OggReader::cleanup()
{
	if (audioHeaders == 0) {
		vorbis_dsp_clear(&vd);
		vorbis_block_clear(&vb);
	}

	th_decode_free(theora);

	vorbis_info_clear(&vi);
	vorbis_comment_clear(&vc);
	if (audioSerial != -1) {
		ogg_stream_clear(&vorbisStream);
	}

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
	auto last = vorbisPos;
	for (const auto& audioFrag : view::reverse(audioList)) {
		last -= audioFrag->length;
		audioFrag->position = last;
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

		int res = ogg_stream_packetout(&vorbisStream, &packet);
		if (res < 0) {
			throw MSXException("error in vorbis stream");
		}
		if (res == 0) break;

		if (audioHeaders == 0) {
			// ignore, we'll seek to the beginning again before playing
			continue;
		}

		if (packet.packetno <= 2) {
			if (vorbis_synthesis_headerin(&vi, &vc, &packet) < 0) {
				throw MSXException("invalid vorbis header");
			}
			--audioHeaders;
		}

		if (packet.packetno == 2) {
			vorbis_synthesis_init(&vd, &vi) ;
			vorbis_block_init(&vd, &vb);
		}
	}
}

void OggReader::theoraHeaderPage(ogg_page* page, th_info& ti, th_comment& tc,
				 th_setup_info*& tsi)
{
	ogg_stream_pagein(&theoraStream, page);

	while (true) {
		ogg_packet packet;

		int res = ogg_stream_packetout(&theoraStream, &packet);
		if (res < 0) {
			throw MSXException("error in vorbis stream");
		}
		if (res == 0) break;

		if (theora) {
			// ignore, we'll seek to the beginning again before playing
			continue;
		}

		if (packet.packetno <= 2) {
			res = th_decode_headerin(&ti, &tc, &tsi, &packet);
			if (res <= 0) {
				throw MSXException("invalid theora header");
			}
		}

		if (packet.packetno == 2) {
			theora = th_decode_alloc(&ti, tsi);
			readMetadata(tc);
			granuleShift = ti.keyframe_granule_shift;
		}
	}
}

void OggReader::readVorbis(ogg_packet* packet)
{
	// deal with header packets
	if (packet->packetno <= 2) [[unlikely]] {
		return;
	}

	if (state == FIND_LAST) {
		if (packet->granulepos != -1) {
			if ((currentSample == AudioFragment::UNKNOWN_POS) ||
			    (size_t(packet->granulepos) > currentSample)) {
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
	int decoded = vorbis_synthesis_pcmout(&vd, &pcm);
	int pos = 0;

	while (pos < decoded)  {
		// Find memory to copy PCM into
		if (recycleAudioList.empty()) {
			auto audio = std::make_unique<AudioFragment>();
			audio->length = 0;
			recycleAudioList.push_back(std::move(audio));
		}
		auto& audio = recycleAudioList.front();
		if (audio->length == 0) {
			audio->position = vorbisPos;
		} else {
			// first element was already partially filled
		}

		// Copy PCM
		auto len = std::min(decoded - pos,
		                    narrow<int>(AudioFragment::MAX_SAMPLES - audio->length));

		ranges::copy(std::span{&pcm[0][pos], size_t(len)}, &audio->pcm[0][audio->length]);
		ranges::copy(std::span{&pcm[1][pos], size_t(len)}, &audio->pcm[1][audio->length]);
		audio->length += len;
		pos += len;

		// Last packet or found position after seeking?
		bool last = (decoded == pos && (packet->e_o_s ||
				 (vorbisPos == AudioFragment::UNKNOWN_POS &&
				  packet->granulepos != -1)));

		if (vorbisPos != AudioFragment::UNKNOWN_POS) {
			vorbisPos += len;
			currentSample += len;
		}

		if (audio->length == AudioFragment::MAX_SAMPLES || last) {
			audioList.push_back(recycleAudioList.pop_front());
		}
	}

	// The granulepos is the no. of samples since the beginning of the
	// stream. Only once per ogg page is this populated.
	if (packet->granulepos != -1) {
		if (vorbisPos == AudioFragment::UNKNOWN_POS) {
			vorbisPos = packet->granulepos;
			vorbisFoundPosition();
		} else {
			if (vorbisPos != size_t(packet->granulepos)) {
				cli.printWarning(
					"vorbis audio out of sync, expected ",
					vorbisPos, ", got ", packet->granulepos);
				vorbisPos = packet->granulepos;
			}
		}
	}

	// done with PCM data
	vorbis_synthesis_read(&vd, decoded);
}

size_t OggReader::frameNo(const ogg_packet* packet) const
{
	if (packet->granulepos == -1) {
		return size_t(-1);
	}

	size_t intra = packet->granulepos & ((size_t(1) << granuleShift) - 1);
	size_t key = packet->granulepos >> granuleShift;
	return key + intra;
}

void OggReader::readMetadata(th_comment& tc)
{
	char* metadata = nullptr;
	for (auto i : xrange(tc.comments)) {
		if (!strncasecmp(tc.user_comments[i], "location=",
				 strlen("location="))) {
			metadata = tc.user_comments[i] + strlen("location=");
			break;
		}
	}
	if (!metadata) {
		return;
	}

	// Maybe there is a better way of doing this parsing in C++
	char* p = metadata;
	while (p) {
		while (isspace(*p)) {
			p++;
		}
		if (strncasecmp(p, "chapter: ", 9) == 0) {
			int chapter = atoi(p + 9);
			p = strchr(p, ',');
			if (!p) break;
			++p;
			size_t frame = atol(p);
			if (frame) {
				chapters.emplace_back(ChapterFrame{chapter, frame});
			}
		} else if (strncasecmp(p, "stop: ", 6) == 0) {
			size_t stopFrame = atol(p + 6);
			if (stopFrame) {
				stopFrames.push_back(stopFrame);
			}
		}
		p = strchr(p, '\n');
		if (p) ++p;
	}
	ranges::sort(stopFrames);
	ranges::sort(chapters, {}, &ChapterFrame::chapter);
}

void OggReader::readTheora(ogg_packet* packet)
{
	if (th_packet_isheader(packet)) {
		return;
	}

	size_t frameno = frameNo(packet);

	// If we're seeking, we're only interested in packets with
	// frame numbers
	if ((state != PLAYING) && (frameno == size_t(-1))) {
		return;
	}

	if (state == FIND_LAST) {
		if ((currentFrame == size_t(-1)) || (currentFrame < frameno)) {
			currentFrame = frameno;
		}
		return;

	} else if (state == FIND_FIRST) {
		if ((currentFrame == size_t(-1)) || (currentFrame > frameno)) {
			currentFrame = frameno;
		}
		return;

	} else if (state == FIND_KEYFRAME) {
		if (frameno < currentFrame) {
			keyFrame = packet->granulepos >> granuleShift;
		} else if (currentFrame == frameno) {
			keyFrame = packet->granulepos >> granuleShift;
			currentSample = 1;
		} else if (frameno > currentFrame) {
			currentSample = 1;
		}
		return;
	}

	if ((keyFrame != size_t(-1)) && (frameno != size_t(-1)) &&
	    (frameno < keyFrame)) {
		// We're reading before the keyframe, discard
		return;
	}

	if (packet->bytes == 0 && frameList.empty()) {
		// No use passing empty packets (which represent dup frame)
		// before we've read any frame.
		return;
	}

	keyFrame = size_t(-1);

	int rc = th_decode_packetin(theora, packet, nullptr);
	switch (rc) {
	case TH_DUPFRAME:
		if (frameList.empty()) {
			cli.printWarning("Theora error: dup frame encountered "
					 "without preceding frame");
		} else {
			frameList.back()->length++;
		}
		break;
	case TH_EIMPL:
		cli.printWarning("Theora error: not capable of reading this");
		break;
	case TH_EFAULT:
		cli.printWarning("Theora error: API not used correctly");
		break;
	case TH_EBADPACKET:
		cli.printWarning("Theora error: bad packet");
		break;
	case 0:
		break;
	default:
		cli.printWarning("Theora error: unknown error ", rc);
		break;
	}

	if (rc) {
		return;
	}

	th_ycbcr_buffer yuv;

	if (th_decode_ycbcr_out(theora, yuv) != 0) {
		return;
	}

	if ((frameno != size_t(-1)) && (frameno < currentFrame)) {
		return;
	}

	currentFrame = frameno + 1;

	std::unique_ptr<Frame> frame;
	if (recycleFrameList.empty()) {
		frame = std::make_unique<Frame>(yuv);
	} else {
		frame = std::move(recycleFrameList.back());
		recycleFrameList.pop_back();
	}

	size_t y_size  = yuv[0].height * size_t(yuv[0].stride);
	size_t uv_size = yuv[1].height * size_t(yuv[1].stride);
	ranges::copy(std::span{yuv[0].data,  y_size}, frame->buffer[0].data);
	ranges::copy(std::span{yuv[1].data, uv_size}, frame->buffer[1].data);
	ranges::copy(std::span{yuv[2].data, uv_size}, frame->buffer[2].data);

	// At lot of frames have frame number -1, only some have the correct
	// frame number. We continue counting from the previous known
	// postion
	Frame* last = frameList.empty() ? nullptr : frameList.back().get();
	if (last && (last->no != size_t(-1))) {
		if (frameno != one_of(size_t(-1), last->no + last->length)) {
			cli.printWarning("Theora frame sequence wrong");
		} else {
			frameno = last->no + last->length;
		}
	}

	frame->no = frameno;
	frame->length = 1;

	// We may read some frames before we encounter one with a proper
	// frame number. When we do, go back and populate the frame
	// numbers correctly
	if (!frameList.empty() && (frameno != size_t(-1)) &&
	    (frameList[0]->no == size_t(-1))) {
		for (auto& frm : view::reverse(frameList)) {
			frameno -= frm->length;
			frm->no = frameno;
		}
	}

	frameList.push_back(std::move(frame));
}

void OggReader::getFrameNo(RawFrame& rawFrame, size_t frameno)
{
	Frame* frame;
	while (true) {
		// If there are no frames or the frames we have read
		// does not include a proper frame number, just read
		// more data
		if (frameList.empty() || (frameList[0]->no == size_t(-1))) {
			if (!nextPacket()) {
				return;
			}
			continue;
		}

		// Remove unneeded frames. Note that at 60Hz the odd and
		// and even frame are displayed during still, so we can
		// only throw away the one two frames ago
		while (frameList.size() >= 3 && frameList[2]->no <= frameno) {
			recycleFrameList.push_back(frameList.pop_front());
		}

		if (!frameList.empty() && frameList[0]->no > frameno) {
			// we're missing frames!
			frame = frameList[0].get();
			cli.printWarning(
					"Cannot find frame ", frameno, " using ",
			        frame->no, " instead");
			break;
		}

		if ((frameList.size() >= 2) &&
		    ((frameno >= frameList[0]->no) &&
		     (frameno <  frameList[1]->no))) {
			frame = frameList[0].get();
			break;
		}

		if ((frameList.size() >= 3) &&
		    ((frameno >= frameList[1]->no) &&
		     (frameno <  frameList[2]->no))) {
			frame = frameList[1].get();
			break;
		}

		// Sanity check, should not happen
		if (frameList.size() > (size_t(2) << granuleShift)) {
			// We've got more than twice as many frames
			// as the maximum distance between key frames.
			cli.printWarning("Cannot find frame ", frameno);
			return;
		}

		// ..add read some new ones
		if (!nextPacket()) {
			return;
		}
	}

	yuv2rgb::convert(frame->buffer, rawFrame);
}

void OggReader::recycleAudio(std::unique_ptr<AudioFragment> audio)
{
	audio->length = 0;
	recycleAudioList.push_back(std::move(audio));
}

const AudioFragment* OggReader::getAudio(size_t sample)
{
	// Read while position is unknown
	while (audioList.empty() ||
	       audioList.front()->position == AudioFragment::UNKNOWN_POS) {
		if (!nextPacket()) {
			return nullptr;
		}
	}

	auto it = begin(audioList);
	while (true) {
		const auto& audio = *it;
		if (audio->position + audio->length + getSampleRate() <= sample) {
			// Dispose if this, more than 1 second old
			recycleAudio(std::move(*it));
			it = audioList.erase(it);
		} else if (audio->position + audio->length <= sample) {
			++it;
		} else {
			if (audio->position <= sample) {
				return audio.get();
			} else {
				// gone too far?
				return nullptr;
			}
		}

		// read more if we're at the end of the list
		if (it == end(audioList)) {
			size_t size = audioList.size();
			while (size == audioList.size()) {
				if (!nextPacket()) {
					return nullptr;
				}
			}

			// reset the iterator to not point to the end
			it = begin(audioList);
		}
	}
}

bool OggReader::nextPacket()
{
	ogg_packet packet;
	ogg_page page;

	while (true) {
		int ret = ogg_stream_packetout(&vorbisStream, &packet);
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
			cli.printWarning("Unexpected stream with serial ",
			                 serial, " in ogg file");
		}
	}
}


bool OggReader::nextPage(ogg_page* page)
{
	static constexpr size_t CHUNK = 4096;

	long ret;
	while ((ret = ogg_sync_pageseek(&sync, page)) <= 0) {
		if (ret < 0) {
			//throw MSXException("Invalid Ogg file");
		}

		size_t chunk;
		if (fileSize - fileOffset >= CHUNK) {
			chunk = CHUNK;
		} else if (fileOffset < fileSize) {
			chunk = fileSize - fileOffset;
		} else {
			return false;
		}

		char* buffer = ogg_sync_buffer(&sync, long(chunk));
		file.read(std::span{buffer, chunk});
		fileOffset += chunk;

		if (ogg_sync_wrote(&sync, long(chunk)) == -1) {
			cli.printWarning("Internal error: ogg_sync_wrote failed");
		}
	}

	return true;
}

size_t OggReader::bisection(
	size_t frame, size_t sample,
	size_t maxOffset, size_t maxSamples, size_t maxFrames)
{
	// Defined to be a power-of-two such that the calculations can be done faster.
	// Note that the sample-number is in the range of: 1..(44100*60*60)
	constexpr uint64_t SHIFT = 0x20000000ULL;

	uint64_t offsetA = 0, offsetB = maxOffset;
	uint64_t sampleA = 0, sampleB = maxSamples;
	uint64_t frameA = 1, frameB = maxFrames;

	while (true) {
		uint64_t ratio = (frame - frameA) * SHIFT / (frameB - frameA);
		if (ratio < 5) {
			return offsetA;
		}

		uint64_t frameOffset = ratio * (offsetB - offsetA) / SHIFT + offsetA;
		ratio = (sample - sampleA) * SHIFT / (sampleB - sampleA);
		if (ratio < 5) {
			return offsetA;
		}
		uint64_t sampleOffset = ratio * (offsetB - offsetA) / SHIFT + offsetA;
		auto offset = std::min(sampleOffset, frameOffset);

		file.seek(offset);
		fileOffset = offset;
		ogg_sync_reset(&sync);
		currentFrame = size_t(-1);
		currentSample = AudioFragment::UNKNOWN_POS;
		state = FIND_FIRST;

		while (((currentFrame == size_t(-1)) ||
			(currentSample == AudioFragment::UNKNOWN_POS)) &&
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
			return offset;
		}
	}
}

size_t OggReader::findOffset(size_t frame, size_t sample)
{
	static constexpr size_t STEP = 32 * 1024;

	// first calculate total length in bytes, samples and frames

	// The file might have changed since we last requested its size,
	// we assume that only data will be added to it and the ogg streams
	// are exactly as before
	fileSize = file.getSize();
	auto offset = fileSize - 1;

	while (offset > 0) {
		if (offset > STEP) {
			offset -= STEP;
		} else {
			offset = 0;
		}

		file.seek(offset);
		fileOffset = offset;
		ogg_sync_reset(&sync);
		currentFrame = size_t(-1);
		currentSample = AudioFragment::UNKNOWN_POS;
		state = FIND_LAST;

		while (nextPacket()) {
			// continue reading
		}

		state = PLAYING;

		if ((currentFrame != size_t(-1)) &&
		    (currentSample != AudioFragment::UNKNOWN_POS)) {
			break;
		}
	}

	totalFrames = currentFrame;

	// If we're close to beginning, don't bother searching for it,
	// just start at the beginning (arbitrary boundary of 1 second).
	if (sample < getSampleRate() || frame <= 30) {
		keyFrame = 1;
		return 0;
	}

	auto maxOffset = offset;
	auto maxSamples = currentSample;
	auto maxFrames = currentFrame;

	if ((sample > maxSamples) || (frame > maxFrames)) {
		sample = maxSamples;
		frame = maxFrames;
	}

	offset = bisection(frame, sample, maxOffset, maxSamples, maxFrames);

	// Find key frame
	file.seek(offset);
	fileOffset = offset;
	ogg_sync_reset(&sync);
	currentFrame = frame;
	currentSample = 0;
	keyFrame = size_t(-1);
	state = FIND_KEYFRAME;

	while (currentSample == 0 && nextPacket()) {
		// continue reading
	}

	state = PLAYING;

	if (keyFrame == one_of(size_t(-1), frame)) {
		return offset;
	}

	return bisection(keyFrame, sample, maxOffset, maxSamples, maxFrames);
}

bool OggReader::seek(size_t frame, size_t samples)
{
	// Remove all queued frames
	recycleFrameList.insert(end(recycleFrameList),
		std::move_iterator(begin(frameList)),
		std::move_iterator(end  (frameList)));
	frameList.clear();

	// Remove all queued audio
	if (!recycleAudioList.empty()) {
		recycleAudioList.front()->length = 0;
	}
	for (auto& a : audioList) {
		recycleAudio(std::move(a));
	}
	audioList.clear();

	fileOffset = findOffset(frame, samples);
	file.seek(fileOffset);

	ogg_sync_reset(&sync);

	vorbisPos = AudioFragment::UNKNOWN_POS;
	currentFrame = frame;
	currentSample = samples;

	vorbis_synthesis_restart(&vd);

	return true;
}

bool OggReader::stopFrame(size_t frame) const
{
	return ranges::binary_search(stopFrames, frame);
}

size_t OggReader::getChapter(int chapterNo) const
{
	auto c = binary_find(chapters, chapterNo, {}, &ChapterFrame::chapter);
	return c ? c->frame : 0;
}

} // namespace openmsx
