// $Id$

#ifndef ZLIBINFLATE_HH
#define ZLIBINFLATE_HH

#include "openmsx.hh"
#include <string>
#include <zlib.h>

namespace openmsx {

template<typename T> class MemBuffer;

class ZlibInflate
{
public:
	ZlibInflate(const byte* buffer, unsigned len);
	~ZlibInflate();

	void skip(unsigned num);
	byte getByte();
	unsigned get16LE();
	unsigned get32LE();
	std::string getString(unsigned len);
	std::string getCString();

	void inflate(MemBuffer<byte>& output, unsigned sizeHint = 65536);

private:
	z_stream s;
	bool wasInit;
};

} // namespace openmsx

#endif
