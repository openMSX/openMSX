#ifndef ZLIBINFLATE_HH
#define ZLIBINFLATE_HH

#include "MemBuffer.hh"

#include <cstdint>
#include <span>
#include <string>
#include <zlib.h>

namespace openmsx {

class ZlibInflate
{
public:
	explicit ZlibInflate(std::span<const uint8_t> input);
	ZlibInflate(const ZlibInflate&) = delete;
	ZlibInflate(ZlibInflate&&) = delete;
	ZlibInflate& operator=(const ZlibInflate&) = delete;
	ZlibInflate& operator=(ZlibInflate&&) = delete;
	~ZlibInflate();

	void skip(size_t num);
	[[nodiscard]] uint8_t getByte();
	[[nodiscard]] unsigned get16LE();
	[[nodiscard]] unsigned get32LE();
	[[nodiscard]] std::string getString(size_t len);
	[[nodiscard]] std::string getCString();

	[[nodiscard]] MemBuffer<uint8_t> inflate(size_t sizeHint = 65536);

private:
	z_stream s;
	bool wasInit;
};

} // namespace openmsx

#endif
