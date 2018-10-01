#ifndef COLECOSUPERGAMEMODULE_HH
#define COLECOSUPERGAMEMODULE_HH

#include "MSXDevice.hh"
#include "AY8910.hh"
#include "Rom.hh"
#include "CheckedRam.hh"

namespace openmsx {

class ColecoSuperGameModule final : public MSXDevice
{
public:
	ColecoSuperGameModule(const DeviceConfig& config);
	~ColecoSuperGameModule();

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
	unsigned translateMainRamAddress(unsigned address) const;

	AY8910 psg;
	CheckedRam sgmRam;
	CheckedRam mainRam;
	Rom biosRom;
	byte psgLatch;
	bool ramEnabled;
	bool ramAtBiosEnabled;
};

} // namespace openmsx

#endif
