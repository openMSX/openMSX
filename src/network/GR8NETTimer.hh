#ifndef GR8NETTIMER_HH
#define GR8NETTIMER_HH

#include "MSXDevice.hh"

#include "Clock.hh"

#include <cstdint>

namespace openmsx {

/** Not a GR8NET: only the 24-bit counter of that card's controlled interrupt
  * generator (GR8NET manual, section 11.5.1), as a timer. Meant for
  * developers who need a fine clock to time other hardware on a real MSX
  * and in openMSX with the same code.
  *  - The counter is read at 5FEBh (LSB), 5FECh and 5FEDh (MSB), in page 1.
  *    Reading the LSB latches the other two bytes.
  *  - It counts down at 3.579545 MHz (the Z80's clock), as the real one
  *    does with CLS = 1, and wraps every 2^24 ticks.
  *  - It always runs. Writes are ignored, so the control register (5FEAh)
  *    and the period have no effect; on the real card software has to
  *    start the counter (OP = 1, CLS = 1) with a period of 0.
  * Everything else on the card is absent: the I/O ports (so no adapter
  * identification), the generator's interrupt and its INT flag, the 10 MHz
  * clock, the ROM, the mapper, the network and the audio.
  */
class GR8NETTimer final : public MSXDevice
{
public:
	explicit GR8NETTimer(const DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint32_t getCounter(EmuTime time) const;

private:
	Clock<3579545> clock;
	uint32_t latch = 0; // the 24-bit count, latched on the LSB read
};

} // namespace openmsx

#endif
