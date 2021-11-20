#ifndef CRC16_HH
#define CRC16_HH

#include "xrange.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace openmsx {

/**
 * This class calculates CRC numbers for the polygon
 *   x^16 + x^12 + x^5 + 1
 */
class CRC16
{
public:
	/** Create CRC16 with an optional initial value
	 */
	explicit constexpr CRC16(uint16_t initialCRC = 0xffff)
		: crc(initialCRC)
	{
	}

	/** (Re)initialize the current value
	 */
	constexpr void init(uint16_t initialCRC)
	{
		crc = initialCRC;
	}

	constexpr void init(std::initializer_list<uint8_t> list)
	{
		crc = 0xffff;
		for (auto& val : list) {
			update(val);
		}
	}

	/** Update CRC with one byte
	 */
	constexpr void update(uint8_t value)
	{
		// Classical byte-at-a-time algorithm by Dilip V. Sarwate
		crc = (crc << 8) ^ tab[0][(crc >> 8) ^ value];
	}

	/** For large blocks (e.g. 512 bytes) this routine is approx 5x faster
	  * than calling the method above in a loop.
	  */
	constexpr void update(const uint8_t* data, size_t size)
	{
		// Based on:
		//   Slicing-by-4 and slicing-by-8 algorithms by Michael E.
		//   Kounavis and Frank L. Berry from Intel Corp.
		//   http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
		// and the implementation by Peter Kankowski found on:
		//   http://www.strchr.com/crc32_popcnt
		// I transformed the code from CRC32 to CRC16 (this was not
		// trivial because both CRCs use a different convention about
		// bit order). I also made the code work on bigendian hosts.

		unsigned c = crc; // 32-bit are faster than 16-bit calculations
		                  // on x86 and many other modern architectures
		// calculate the bulk of the data 8 bytes at a time
		for (auto n = size / 8; n; --n) {
			c = tab[7][data[0] ^ (c >>  8)] ^
			    tab[6][data[1] ^ (c & 255)] ^
			    tab[5][data[2]] ^
			    tab[4][data[3]] ^
			    tab[3][data[4]] ^
			    tab[2][data[5]] ^
			    tab[1][data[6]] ^
			    tab[0][data[7]];
			data += 8;
		}
		// calculate the remaining bytes in the usual way
		for (size &= 7; size; --size) {
			c = uint16_t(c << 8) ^ tab[0][(c >> 8) ^ *data++];
		}
		crc = c; // store back in a 16-bit result
	}

	/** Get current CRC value
	 */
	[[nodiscard]] constexpr uint16_t getValue() const
	{
		return crc;
	}

private:
	static inline constexpr auto tab = [] {
		std::array<std::array<uint16_t, 0x100>, 8> result = {}; // uint16_t[8][0x100]
		for (auto i : xrange(0x100)) {
			uint16_t x = i << 8;
			repeat(8, [&] {
				x = (x << 1) ^ ((x & 0x8000) ? 0x1021 : 0);
			});
			result[0][i] = x;
		}
		for (auto i : xrange(0x100)) {
			uint16_t c = result[0][i];
			for (auto j : xrange(1, 8)) {
				c = result[0][c >> 8] ^ (c << 8);
				result[j][i] = c;
			}
		}
		return result;
	}();

	uint16_t crc;
};

} // namespace openmsx

#endif
