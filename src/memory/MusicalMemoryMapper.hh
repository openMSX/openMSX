#ifndef MUSICALMEMORYMAPPER_HH
#define MUSICALMEMORYMAPPER_HH

#include "MSXMemoryMapperBase.hh"
#include "SN76489.hh"

namespace openmsx {

/** Memory mapper which also controls an SN76489AN sound chip.
  *
  * http://map.grauw.nl/resources/sound/musical-memory-mapper.pdf
  */
class MusicalMemoryMapper final : public MSXMemoryMapperBase
{
public:
	MusicalMemoryMapper(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	/** Attempts to read a register at the given address.
	  * Returns the register value, or -1 if no register exists at the given
	  * address or register access is disabled.
	  */
	[[nodiscard]] int readReg(word address) const;

	/** Returns true iff a mapper or control register is accessible in the
	  * 256-byte block containing the given address.
	  */
	[[nodiscard]] bool registerAccessAt(word address) const;

	/** Returns true iff the page at the given address is currently write
	  * protected.
	  */
	[[nodiscard]] bool writeProtected(word address) const {
		unsigned page = address >> 14;
		return (controlReg & (1 << page)) != 0;
	}

	void updateControlReg(byte value);

private:
	SN76489 sn76489;
	byte controlReg;
};

} // namespace openmsx

#endif
