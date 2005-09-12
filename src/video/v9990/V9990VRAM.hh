// $Id$

#ifndef V9990VRAM_HH
#define V9990VRAM_HH

#include "Ram.hh"
#include "openmsx.hh"

namespace openmsx {

class EmuTime;
class V9990;

/** Video RAM for the V9990.
  */
class V9990VRAM
{
public:
	/** VRAM Size
	  */
	static const unsigned VRAM_SIZE = 512 * 1024; // 512kB

	/** Construct V9990 VRAM.
	  * @param vdp The V9990 vdp this VRAM belongs to
	  * @param time  Moment in time to create the VRAM
	  */
	V9990VRAM(V9990& vdp, const EmuTime& time);

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to synchronise VRAM to
	  */
	inline void sync(const EmuTime& time) {
		if (&time); // avoid warning
		// not much to do, yet
	}

	static inline unsigned transformBx(unsigned address) {
		return ((address & 1) << 18) | ((address & 0x7FFFE) >> 1);
	}
	static inline unsigned transformP1(unsigned address) {
		return address;
	}
	static inline unsigned transformP2(unsigned address) {
		// TODO check this
		if (address < 0x78000) {
			return transformBx(address);
		} else if (address < 0x7C000) {
			return address - 0x3C000;
		} else {
			return address;
		}
	}

	inline byte readVRAMBx(unsigned address) {
		return data[transformBx(address)];
	}
	inline byte readVRAMP1(unsigned address) {
		return data[transformP1(address)];
	}
	inline byte readVRAMP2(unsigned address) {
		return data[transformP2(address)];
	}

	inline void writeVRAMBx(unsigned address, byte value) {
		data[transformBx(address)] = value;
	}
	inline void writeVRAMP1(unsigned address, byte value) {
		data[transformP1(address)] = value;
	}
	inline void writeVRAMP2(unsigned address, byte value) {
		data[transformP2(address)] = value;
	}

	inline byte readVRAMDirect(unsigned address) {
		return data[address];
	}
	inline void writeVRAMDirect(unsigned address, byte value) {
		data[address] = value;
	}

	byte readVRAMSlow(unsigned address);
	void writeVRAMSlow(unsigned address, byte val);

private:
	unsigned mapAddress(unsigned address);

	/** V9990 VDP this VRAM belongs to.
	  */
	V9990& vdp;

	/** Pointer V9990 VRAM data.
	  */
	Ram data;
};

} // namespace openmsx

#endif
