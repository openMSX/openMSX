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
