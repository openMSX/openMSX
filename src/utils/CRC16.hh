// $Id$

#ifndef CRC16_HH
#define CRC16_HH

#include "openmsx.hh"

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
		crc = (crc << 8) ^ CRC16Table[(crc >> 8) ^ value];
	}
	void update(const byte* data, unsigned size)
	{
		for (unsigned i = 0; i < size; ++i) {
			update(data[i]);
		}
	}

	/** Get current CRC value
	 */
	word getValue() const
	{
		return crc;
	}

private:
	word crc;
	static const word CRC16Table[256];

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
