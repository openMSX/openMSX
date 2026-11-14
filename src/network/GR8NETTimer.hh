#ifndef GR8NETTIMER_HH
#define GR8NETTIMER_HH

#include "MSXDevice.hh"

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

/** Not a GR8NET: only that card's controlled interrupt generator, usable as
  * a timer, without its interrupt. Meant for developers who need a fine
  * clock to time other hardware.
  *  - Special control registers 5FEA (control) and 5FEB-5FED (24-bit
  *    counter), in bank 0 (4000h-5FFFh). All other memory reads FFh.
  * Not emulated: the I/O ports (5Eh/5Fh, so no adapter identification), the
  * ROM, the mapper, the network, the audio, the hardware interrupt (IGE)
  * and its watchdog (WDE), and the Hz mode (HZP). See the GR8NET manual,
  * section 11.5.1 (the generator).
  */
class GR8NETTimer final : public MSXDevice
{
public:
	explicit GR8NETTimer(const DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct State {
		uint32_t left; // ticks left in the current period
		bool expired;  // a period ended since 'start'
	};
	[[nodiscard]] State calc(EmuTime time) const;
	[[nodiscard]] uint32_t period() const;
	void sync(EmuTime time);

private:
	byte control;   // OP, HZP, WDE, IGE and CLS, as written
	bool intFlag;   // INT: a period ended, up to 'start'
	uint32_t latch; // the 24-bit period, latched on the MSB write
	byte writeLow, writeMid;
	byte readMid, readHigh; // latched on the LSB read

	// 'counter' ticks were left in the current period at 'start'
	EmuTime start = EmuTime::zero();
	uint32_t counter;
};

} // namespace openmsx

#endif
