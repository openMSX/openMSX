#ifndef V9990VRAM_HH
#define V9990VRAM_HH

#include "V9990CmdEngine.hh"
#include "Ram.hh"
#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

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
	V9990VRAM(V9990& vdp, EmuTime::param time);

	void clear();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to synchronise VRAM to
	  */
	inline void sync(EmuTime::param time) {
		cmdEngine->sync(time);
	}

	static inline unsigned transformBx(unsigned address) {
		return ((address & 1) << 18) | ((address & 0x7FFFE) >> 1);
	}
	static inline unsigned transformP1(unsigned address) {
		return address;
	}
	static inline unsigned transformP2(unsigned address) {
		// Verified on a real Graphics9000
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

	byte readVRAMCPU(unsigned address, EmuTime::param time);
	void writeVRAMCPU(unsigned address, byte val, EmuTime::param time);

	void setCmdEngine(V9990CmdEngine& cmdEngine_) { cmdEngine = &cmdEngine_; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	unsigned mapAddress(unsigned address);

	/** V9990 VDP this VRAM belongs to.
	  */
	V9990& vdp;

	V9990CmdEngine* cmdEngine;

	/** V9990 VRAM data.
	  */
	Ram data;
};

} // namespace openmsx

#endif
