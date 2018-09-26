#ifndef ZLIBINFLATE_HH
#define ZLIBINFLATE_HH

#include "MemBuffer.hh"
#include "span.hh"
#include <cstdint>
#include <string>
#include <zlib.h>

namespace openmsx {

class ZlibInflate
{
public:
	ZlibInflate(span<uint8_t> input);
	~ZlibInflate();

	void skip(size_t num);
	uint8_t getByte();
	unsigned get16LE();
	unsigned get32LE();
	std::string getString(size_t len);
	std::string getCString();

	size_t inflate(MemBuffer<uint8_t>& output, size_t sizeHint = 65536);

private:
	z_stream s;
	bool wasInit;
};

} // namespace openmsx

#endif
