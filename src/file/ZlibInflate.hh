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
	ZlibInflate(span<const uint8_t> input);
	~ZlibInflate();

	void skip(size_t num);
	[[nodiscard]] uint8_t getByte();
	[[nodiscard]] unsigned get16LE();
	[[nodiscard]] unsigned get32LE();
	[[nodiscard]] std::string getString(size_t len);
	[[nodiscard]] std::string getCString();

	[[nodiscard]] size_t inflate(MemBuffer<uint8_t>& output, size_t sizeHint = 65536);

private:
	z_stream s;
	bool wasInit;
};

} // namespace openmsx

#endif
