#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "File.hh"
#include "circular_buffer.hh"
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/theoradec.h>
#include <memory>
#include <list>
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
	float pcm[2][MAX_SAMPLES];
};

struct Frame
{
	explicit Frame(const th_ycbcr_buffer& yuv);
	~Frame();

	th_ycbcr_buffer buffer;
	size_t no;
	int length;
};

class OggReader
{
public:
	OggReader(const OggReader&) = delete;
	OggReader& operator=(const OggReader&) = delete;

	OggReader(const Filename& filename, CliComm& cli);
	~OggReader();

	bool seek(size_t frame, size_t sample);
	[[nodiscard]] unsigned getSampleRate() const { return vi.rate; }
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
	size_t frameNo(ogg_packet* packet) const;

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
	} state;

	// ogg state
	ogg_sync_state sync;
	ogg_stream_state vorbisStream, theoraStream;
	int audioSerial;
	int videoSerial;
	int skeletonSerial;
	size_t fileOffset;
	size_t fileSize;

	// video
	th_dec_ctx* theora;
	int frameRate;
	size_t keyFrame;
	size_t currentFrame;
	int granuleShift;
	size_t totalFrames;

	cb_queue<std::unique_ptr<Frame>> frameList;
	std::vector<std::unique_ptr<Frame>> recycleFrameList;

	// audio
	int audioHeaders;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
	size_t currentSample;
	size_t vorbisPos;

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
