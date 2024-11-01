#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "File.hh"
#include "circular_buffer.hh"
#include "narrow.hh"
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/theoradec.h>
#include <array>
#include <list>
#include <memory>
#include <vector>

namespace openmsx {

class CliComm;
class RawFrame;
class Filename;

struct AudioFragment
{
	static constexpr size_t UNKNOWN_POS = size_t(-1);
	static constexpr unsigned MAX_SAMPLES = 2048;
	size_t position;
	unsigned length;
	std::array<std::array<float, MAX_SAMPLES>, 2> pcm;
};

struct Frame
{
	explicit Frame(const th_ycbcr_buffer& yuv);
	Frame(const Frame&) = delete;
	Frame(Frame&&) = delete;
	Frame& operator=(const Frame&) = delete;
	Frame& operator=(Frame&&) = delete;
	~Frame();

	th_ycbcr_buffer buffer;
	size_t no;
	int length;
};

class OggReader
{
public:
	OggReader(const Filename& filename, CliComm& cli);
	OggReader(const OggReader&) = delete;
	OggReader(OggReader&&) = delete;
	OggReader& operator=(const OggReader&) = delete;
	OggReader& operator=(OggReader&&) = delete;
	~OggReader();

	bool seek(size_t frame, size_t sample);
	[[nodiscard]] unsigned getSampleRate() const { return narrow<unsigned>(vi.rate); }
	void getFrameNo(RawFrame& frame, size_t frameno);
	[[nodiscard]] const AudioFragment* getAudio(size_t sample);
	[[nodiscard]] size_t getFrames() const { return totalFrames; }
	[[nodiscard]] int getFrameRate() const { return frameRate; }

	// metadata
	[[nodiscard]] bool stopFrame(size_t frame) const;
	[[nodiscard]] size_t getChapter(int chapterNo) const;

private:
	void cleanup();
	void readTheora(ogg_packet* packet);
	void theoraHeaderPage(ogg_page* page, th_info& ti, th_comment& tc,
	                      th_setup_info*& tsi);
	void readMetadata(th_comment& tc);
	void readVorbis(ogg_packet* packet);
	void vorbisHeaderPage(ogg_page* page);
	bool nextPage(ogg_page* page);
	bool nextPacket();
	void recycleAudio(std::unique_ptr<AudioFragment> audio);
	void vorbisFoundPosition();
	size_t frameNo(const ogg_packet* packet) const;

	size_t findOffset(size_t frame, size_t sample);
	size_t bisection(size_t frame, size_t sample,
	                 size_t maxOffset, size_t maxSamples, size_t maxFrames);

private:
	CliComm& cli;
	File file;

	enum State {
		PLAYING,
		FIND_LAST,
		FIND_FIRST,
		FIND_KEYFRAME
	} state{PLAYING};

	// ogg state
	ogg_sync_state sync;
	ogg_stream_state vorbisStream, theoraStream;
	int audioSerial{-1};
	int videoSerial{-1};
	int skeletonSerial{-1};
	size_t fileOffset{0};
	size_t fileSize;

	// video
	th_dec_ctx* theora{nullptr};
	int frameRate;
	size_t keyFrame{size_t(-1)};
	size_t currentFrame{1};
	int granuleShift;
	size_t totalFrames;

	cb_queue<std::unique_ptr<Frame>> frameList;
	std::vector<std::unique_ptr<Frame>> recycleFrameList;

	// audio
	int audioHeaders{3};
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
	size_t currentSample{0};
	size_t vorbisPos{0};

	std::list<std::unique_ptr<AudioFragment>> audioList;
	cb_queue<std::unique_ptr<AudioFragment>> recycleAudioList;

	// Metadata
	std::vector<size_t> stopFrames;
	struct ChapterFrame {
		int chapter;
		size_t frame;
	};
	std::vector<ChapterFrame> chapters; // sorted on chapter
};

} // namespace openmsx

#endif
