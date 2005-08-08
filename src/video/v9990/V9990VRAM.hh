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

	byte readVRAM(unsigned address);
	void writeVRAM(unsigned address, byte val);
	byte readVRAMInterleave(unsigned address);
	void writeVRAMInterleave(unsigned address, byte val);
	inline byte readVRAMP1(unsigned address) { return data[address]; }

private:
	/** V9990 VDP this VRAM belongs to.
	  */
	V9990& vdp;

	/** Pointer V9990 VRAM data.
	  */
	Ram data;
};

} // namespace openmsx

#endif
