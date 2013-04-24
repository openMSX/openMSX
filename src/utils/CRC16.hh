#ifndef CRC16_HH
#define CRC16_HH

#include "openmsx.hh"
#include <cstring> // for size_t

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
	explicit CRC16(word initialCRC = 0xFFFF)
	{
		init(initialCRC);
	}

	/** (Re)initialize the current value
	 */
	void init(word initialCRC)
	{
		crc = initialCRC;
	}

	/** (Re)initialize with a short initial sequence.
	  * The initial value is guaranteed to be computed at compile time.
	  */
	template<byte V1> void init()
	{
		static const word T0 = 0xffff;
		static const word T1 = CT_CRC16<T0, V1>::value;
		init(T1);
	}
	template<byte V1, byte V2> void init()
	{
		static const word T0 = 0xffff;
		static const word T1 = CT_CRC16<T0, V1>::value;
		static const word T2 = CT_CRC16<T1, V2>::value;
		init(T2);
	}
	template<byte V1, byte V2, byte V3> void init()
	{
		static const word T0 = 0xffff;
		static const word T1 = CT_CRC16<T0, V1>::value;
		static const word T2 = CT_CRC16<T1, V2>::value;
		static const word T3 = CT_CRC16<T2, V3>::value;
		init(T3);
	}
	template<byte V1, byte V2, byte V3, byte V4> void init()
	{
		static const word T0 = 0xffff;
		static const word T1 = CT_CRC16<T0, V1>::value;
		static const word T2 = CT_CRC16<T1, V2>::value;
		static const word T3 = CT_CRC16<T2, V3>::value;
		static const word T4 = CT_CRC16<T3, V4>::value;
		init(T4);
	}

	/** Update CRC with one byte
	 */
	void update(byte value)
	{
		// Classical byte-at-a-time algorithm by Dilip V. Sarwate
		crc = (crc << 8) ^ tab[0][(crc >> 8) ^ value];
	}

	/** For large blocks (e.g. 512 bytes) this routine is approx 5x faster
	  * than calling the method above in a loop.
	  */
	void update(const byte* data, size_t size)
	{
		// Based on:
		//   Slicing-by-4 and slicing-by-8 algorithms by Michael E.
		//   Kounavis and Frank L. Berry from Intel Corp.
		//   http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
		// and the implementation by Peter Kankowski found on:
		//   http://www.strchr.com/crc32_popcnt
		// I transformed the code from CRC32 to CRC16 (this was not
		// trivial because both CRCs use a different convention about
		// bit order). I also made the code also work on bigendian
		// hosts.

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
			c = word(c << 8) ^ tab[0][(c >> 8) ^ *data++];
		}
		crc = c; // store back in a 16-bit result
	}

	/** Get current CRC value
	 */
	word getValue() const
	{
		return crc;
	}

private:
	word crc;
	static const word tab[8][256];

	// The Stuff below is template magic to perform the following
	// computation at compile-time:
	//    for (int i = 8; i < 16; ++i) {
	//        crc = (crc << 1) ^ ((((crc ^ (data << i)) & 0x8000) ? 0x1021 : 0));
	//    }
	template<word C, word V, int B> struct CT_H {
		static const word D = word(C << 1) ^ (((C ^ V) & 0x8000) ? 0x1021 : 0);
		static const word value = CT_H<D, word(V << 1), B - 1>::value;
	};
	template<word C, word V> struct CT_H<C, V, 0> {
		static const word value = C;
	};
	template<word IN, byte VAL> struct CT_CRC16 : CT_H<IN, VAL << 8, 8> {};
};

} // namespace openmsx

#endif
