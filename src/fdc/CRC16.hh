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
	CRC16(word initialCRC = 0xFFFF)
	{
		crc = initialCRC;
	}
	
	/** Update CRC with one byte
	 */
	void update(byte value)
	{
		crc = (crc << 8) ^ CRC16Table[(crc >> 8) ^ value];
	}

	/** Update CRC with a buffer of bytes
	 */
	void update(byte* values, int num)
	{
		while (num--) {
			update(*values++);
		}
	}

	/** Get current CRC value
	 */
	word getValue()
	{
		return crc;
	}

private:
	word crc;
	static const word CRC16Table[256];
};

} // namespace openmsx

#endif
