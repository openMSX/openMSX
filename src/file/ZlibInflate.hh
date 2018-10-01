#ifndef ZLIBINFLATE_HH
#define ZLIBINFLATE_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include <string>
#include <zlib.h>

namespace openmsx {

class ZlibInflate
{
public:
	ZlibInflate(const byte* input, size_t inputLen);
	~ZlibInflate();

	void skip(size_t num);
	byte getByte();
	unsigned get16LE();
	unsigned get32LE();
	std::string getString(size_t len);
	std::string getCString();

	size_t inflate(MemBuffer<byte>& output, size_t sizeHint = 65536);

private:
	z_stream s;
	bool wasInit;
};

} // namespace openmsx

#endif
