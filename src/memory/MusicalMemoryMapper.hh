#ifndef MUSICALMEMORYMAPPER_HH
#define MUSICALMEMORYMAPPER_HH

#include "MSXMemoryMapperBase.hh"
#include <memory>

namespace openmsx {

class SN76489;

/** Memory mapper which also controls an SN76489AN sound chip.
  *
  * http://map.grauw.nl/resources/sound/musical-memory-mapper.pdf
  */
class MusicalMemoryMapper final : public MSXMemoryMapperBase
{
public:
	MusicalMemoryMapper(const DeviceConfig& config);
	~MusicalMemoryMapper() override;

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** Attempts to read a register at the given address.
	  * Returns the register value, or -1 if no register exists at the given
	  * address or register access is disabled.
	  */
	int readReg(word address) const;

	/** Returns true iff a mapper or control register is accessible in the
	  * 256-byte block containing the given address.
	  */
	bool registerAccessAt(word address) const;

	/** Returns true iff the page at the given address is currently write
	  * protected.
	  */
	bool writeProtected(word address) const {
		unsigned page = address >> 14;
		return (controlReg & (1 << page)) != 0;
	}

	void updateControlReg(byte value);

	std::unique_ptr<SN76489> sn76489;
	byte controlReg;
};

} // namespace openmsx

#endif
