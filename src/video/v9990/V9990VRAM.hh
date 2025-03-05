#ifndef V9990VRAM_HH
#define V9990VRAM_HH

#include "V9990CmdEngine.hh"

#include "EmuTime.hh"
#include "TrackedRam.hh"

#include <cstdint>

namespace openmsx {

class V9990;

/** Video RAM for the V9990.
  */
class V9990VRAM
{
public:
	/** VRAM Size
	  */
	static constexpr unsigned VRAM_SIZE = 512 * 1024; // 512kB

	/** Construct V9990 VRAM.
	  * @param vdp The V9990 vdp this VRAM belongs to
	  * @param time  Moment in time to create the VRAM
	  */
	V9990VRAM(V9990& vdp, EmuTime::param time);

	void clear();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to synchronize VRAM to
	  */
	void sync(EmuTime::param time) {
		cmdEngine->sync(time);
	}

	[[nodiscard]] static unsigned transformBx(unsigned address) {
		return ((address & 1) << 18) | ((address & 0x7FFFE) >> 1);
	}
	[[nodiscard]] static unsigned transformP1(unsigned address) {
		return address;
	}
	[[nodiscard]] static unsigned transformP2(unsigned address) {
		// Verified on a real Graphics9000
		if (address < 0x78000) {
			return transformBx(address);
		} else if (address < 0x7C000) {
			return address - 0x3C000;
		} else {
			return address;
		}
	}

	[[nodiscard]] uint8_t readVRAMBx(unsigned address) const {
		return data[transformBx(address)];
	}
	[[nodiscard]] uint8_t readVRAMP1(unsigned address) const {
		return data[transformP1(address)];
	}
	[[nodiscard]] uint8_t readVRAMP2(unsigned address) const {
		return data[transformP2(address)];
	}

	void writeVRAMBx(unsigned address, uint8_t value) {
		data.write(transformBx(address), value);
	}
	void writeVRAMP1(unsigned address, uint8_t value) {
		data.write(transformP1(address), value);
	}
	void writeVRAMP2(unsigned address, uint8_t value) {
		data.write(transformP2(address), value);
	}

	[[nodiscard]] uint8_t readVRAMDirect(unsigned address) const {
		return data[address];
	}
	void writeVRAMDirect(unsigned address, uint8_t value) {
		data.write(address, value);
	}

	[[nodiscard]] uint8_t readVRAMCPU(unsigned address, EmuTime::param time);
	void writeVRAMCPU(unsigned address, uint8_t val, EmuTime::param time);

	void setCmdEngine(V9990CmdEngine& cmdEngine_) { cmdEngine = &cmdEngine_; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned mapAddress(unsigned address) const;

private:
	/** V9990 VDP this VRAM belongs to.
	  */
	V9990& vdp;

	V9990CmdEngine* cmdEngine = nullptr;

	/** V9990 VRAM data.
	  */
	TrackedRam data;
};

} // namespace openmsx

#endif
