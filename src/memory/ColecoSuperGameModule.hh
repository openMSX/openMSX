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
	explicit ColecoSuperGameModule(DeviceConfig& config);
	~ColecoSuperGameModule() override;

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
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
