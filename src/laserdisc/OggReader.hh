// $Id$

#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "openmsx.hh"
#include "LocalFileReference.hh"
#include <oggz/oggz.h>
#include <vorbis/codec.h>
#include <theora/theora.h>
#include <list>
#include <string>

namespace openmsx {

class CliComm;
class RawFrame;

struct AudioFragment
{
	static const unsigned UNKNOWN_POS = 0xffffffff;
	static const unsigned MAX_SAMPLES = 2048;
	unsigned position;
	unsigned length;
	float pcm[2][MAX_SAMPLES];
};

class OggReader
{
public:
	OggReader(const std::string& filename, CliComm& cli_);
	~OggReader();

	bool seek(int frame, int sample);
	unsigned getSampleRate() const { return vi.rate; }
	void getFrame(RawFrame& frame);
	AudioFragment* getAudio(unsigned sample);

private:
	void cleanup();
	void readTheora(ogg_packet* packet);
	void readVorbis(ogg_packet* packet);
	void returnAudio(AudioFragment* audio);
	void vorbisFoundPosition();
	int frameNo(ogg_packet* packet);
	static int readCallback(OGGZ* oggz, ogg_packet* packet, long serial,
							void* userdata);
	static int seekCallback(OGGZ* oggz, ogg_packet* packet, long serial,
							void* userdata);

	CliComm& cli;

	// ogg state
	OGGZ* oggz;
	long audioSerial;
	long videoSerial;

	// video
	int videoHeaders;
	theora_state video_handle;
	theora_info video_info;
	theora_comment video_comment;
	int keyFrame;
	int currentFrame;

	std::list<yuv_buffer*> frameList;
	std::list<yuv_buffer*> recycleFrameList;

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
};

} // namespace openmsx

#endif
