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
	explicit MusicalMemoryMapper(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** Attempts to read a register at the given address.
	  * Returns the register value, or -1 if no register exists at the given
	  * address or register access is disabled.
	  */
	[[nodiscard]] int readReg(uint16_t address) const;

	/** Returns true iff a mapper or control register is accessible in the
	  * 256-byte block containing the given address.
	  */
	[[nodiscard]] bool registerAccessAt(uint16_t address) const;

	/** Returns true iff the page at the given address is currently write
	  * protected.
	  */
	[[nodiscard]] bool writeProtected(uint16_t address) const {
		unsigned page = address >> 14;
		return (controlReg & (1 << page)) != 0;
	}

	void updateControlReg(byte value);

private:
	SN76489 sn76489;
	byte controlReg = 0;
};

} // namespace openmsx

#endif
