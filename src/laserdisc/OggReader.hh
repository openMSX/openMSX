// $Id$

#ifndef OGGREADER_HH
#define OGGREADER_HH

#include "openmsx.hh"

namespace openmsx {

struct Reader;

class OggReader
{
public:
	OggReader(const Filename& filename);
	~OggReader();
	
	bool seek(unsigned pos);
	unsigned getSampleRate();
	unsigned fillFloatBuffer(float ***pcm, unsigned num);
	byte *getFrame();

private:
	void cleanup();

	struct Reader *reader;
};

} // namespace openmsx

#endif

