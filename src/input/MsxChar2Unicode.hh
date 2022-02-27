#ifndef MSXCHAR2UNICODE_HH
#define MSXCHAR2UNICODE_HH

#include "span.hh"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace openmsx {

class MsxChar2Unicode
{
public:
	MsxChar2Unicode(std::string_view mappingName);

	/** TODO */
	[[nodiscard]] std::string msxToUtf8(
		span<const uint8_t> msx,
		std::function<uint32_t(uint8_t)> fallback) const;
	/** TODO */
	[[nodiscard]] std::vector<uint8_t> utf8ToMsx(
		std::string_view utf8,
		std::function<uint8_t(uint32_t)> fallback) const;

private:
	void parseVid(std::string_view file);

private:
	// LUT: msx graphical character code (as stored in VRAM) -> unicode character
	// Some entries are not filled in (are invalid), they contain the value -1.
	// There may be different (valid) entries that contain the same value.
	uint32_t msx2unicode[256];

	// Reverse LUT: unicode character -> msx graphical character
	struct Entry {
		Entry(uint32_t u, uint8_t m) : unicode(u), msx(m) {}
		uint32_t unicode;
		uint8_t msx;
	};
	// Sorted on Entry::unicode. All 'unicode' fields are unique, but
	// there can be different entries that have the same 'msx' field.
	std::vector<Entry> unicode2msx;
};

} // namespace openmsx

#endif
