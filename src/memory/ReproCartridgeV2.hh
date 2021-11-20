#ifndef REPROCARTRIDGEV2_HH
#define REPROCARTRIDGEV2_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "AY8910.hh"

namespace openmsx {

class ReproCartridgeV2 final : public MSXRom
{
public:
	ReproCartridgeV2(const DeviceConfig& config, Rom&& rom);
	~ReproCartridgeV2() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void writeIO(word port, byte value, EmuTime::param time) override;

	void setVolume(EmuTime::param time, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isSCCAccess(word addr) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

private:
	AmdFlash flash;
	SCC scc;
	AY8910 psg0x10, psg0xA0;

	bool flashRomWriteEnabled;
	byte mainBankReg;
	byte volumeReg;
	byte mapperTypeReg;
	byte psg0x10Latch, psg0xA0Latch;
	byte sccMode;
	byte bankRegs[4];
};

} // namespace openmsx

#endif
