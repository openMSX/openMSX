#ifndef GR8NET_HH
#define GR8NET_HH

#include "MSXDevice.hh"

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

/** A minimal GR8NET: only what's needed to use the card's controlled
  * interrupt generator as a timer, without its interrupt.
  *  - I/O ports 5Eh/5Fh: adapter selection, the "GR8N" + date
  *    identification sequence (register 0) and the slot ID (register 1).
  *  - Special control registers 5FEA (control) and 5FEB-5FED (24-bit
  *    counter), in bank 0 (4000h-5FFFh). All other memory reads FFh.
  * Not emulated: the ROM, the mapper, the network, the audio, the hardware
  * interrupt (IGE) and its watchdog (WDE), and the Hz mode (HZP). See the
  * GR8NET manual, sections 11.3 (I/O ports) and 11.5.1 (the generator).
  */
class GR8NET final : public MSXDevice
{
public:
	explicit GR8NET(const DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Position {
		uint32_t value;   // the counter as read (24 bits)
		uint32_t inPeriod; // ticks elapsed in the current period
		uint32_t period;  // the current period's length, in ticks
		uint64_t expiries; // periods completed since 'start'
	};
	[[nodiscard]] Position position(EmuTime time) const;
	[[nodiscard]] uint32_t latchedPeriod() const;
	void sync(EmuTime time);
	void rebase(EmuTime time);
	void writeControl(byte value, EmuTime time);
	[[nodiscard]] byte readControl(EmuTime time) const;
	[[nodiscard]] byte readPort5F() const;

private:
	// I/O port interface
	byte port5E;
	byte idIndex; // position in the register 0 sequence

	// controlled interrupt generator
	byte control;      // bits 2-5 as written (HZP, WDE, IGE, CLS)
	bool running;      // OP
	bool intFlag;      // INT: a period ended since it was last cleared
	bool errFlag;      // ER (never set: no watchdog)
	uint32_t latch;    // the 24-bit value, latched on the MSB write
	byte writeLow, writeMid;
	byte readMid, readHigh; // latched on the LSB read

	// Counting: 'offset' ticks of a 'period'-tick period had elapsed at
	// 'start', at 'freq' Hz; later periods take latchedPeriod() ticks.
	EmuTime start = EmuTime::zero();
	uint32_t offset;
	uint32_t period;
	uint32_t freq;
	uint64_t seenExpiries; // expiries already reflected in 'intFlag'
};

} // namespace openmsx

#endif
