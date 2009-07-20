// $Id$

#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "openmsx.hh"
#include <oggz/oggz.h>
#include <vorbis/codec.h>
#include <theora/theora.h>
#include <string>

namespace openmsx {

class OggReader
{
public:
	OggReader(const std::string& filename);
	~OggReader();

	bool seek(unsigned pos);
	unsigned fillFloatBuffer(float*** pcm, unsigned num);
	unsigned getSampleRate() const { return vi.rate; }
	const byte* getFrame() const { return rawframe; }

private:
	void cleanup();
	void readTheora(ogg_packet* packet);
	void readVorbis(ogg_packet* packet);
	static int readCallback(OGGZ* oggz, ogg_packet* packet,
                                long serial, void* userdata);
	static int seekCallback(OGGZ* oggz, ogg_packet* packet,
                                long serial, void* userdata);

	// ogg state
	OGGZ* oggz;
	long audio_serial;
	long video_serial;

	// video
	int video_header_packets;
	theora_state video_handle;
	theora_info video_info;
	theora_comment video_comment;
	byte* rawframe;
	int intraframes;

	// audio
	int audio_header_packets;
	long readPos, writePos;
	float* pcm[2];
	float* ret[2];
	long pcmSize;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
};

} // namespace openmsx

#endif
