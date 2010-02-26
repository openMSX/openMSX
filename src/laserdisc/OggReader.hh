// $Id$

#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/theoradec.h>
#include <memory>
#include <list>
#include <deque>
#include <map>
#include <set>

namespace openmsx {

class CliComm;
class RawFrame;
class File;
class Filename;

struct AudioFragment
{
	static const unsigned UNKNOWN_POS = 0xffffffff;
	static const unsigned MAX_SAMPLES = 2048;
	unsigned position;
	unsigned length;
	float pcm[2][MAX_SAMPLES];
};

struct Frame
{
	int no;
	th_ycbcr_buffer buffer;
};

class OggReader : private noncopyable
{
public:
	OggReader(const Filename& filename, CliComm& cli);
	~OggReader();

	bool seek(int frame, int sample);
	unsigned getSampleRate() const { return vi.rate; }
	void getFrameNo(RawFrame& frame, int frameno);
	const AudioFragment* getAudio(unsigned sample);
	int getFrames() const;

	// metadata
	bool stopFrame(int frame) const;
	int chapter(int chapterNo) const;

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
	void returnAudio(AudioFragment* audio);
	void vorbisFoundPosition();
	int frameNo(ogg_packet* packet);

	unsigned findOffset(int frame, unsigned sample);
	unsigned bisection(int frame, unsigned sample,
		unsigned maxOffset, unsigned maxSamples, unsigned maxFrames);

	CliComm& cli;
	const std::auto_ptr<File> file;

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
	unsigned fileOffset;
	unsigned fileSize;

	// video
	th_dec_ctx* theora;
	int keyFrame;
	int currentFrame;
	int granuleShift;
	int totalFrames;

	typedef std::deque<Frame*> Frames;
	Frames frameList;
	Frames recycleFrameList;

	// audio
	int audioHeaders;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
	unsigned currentSample;
	unsigned vorbisPos;

	typedef std::list<AudioFragment*> AudioFragments;
	AudioFragments audioList;
	AudioFragments recycleAudioList;

	// Metadata
	std::set<int> stopFrames;
	std::map<int, int> chapters;
};

} // namespace openmsx

#endif
