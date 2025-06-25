#ifndef REPROCARTRIDGEV2_HH
#define REPROCARTRIDGEV2_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "AY8910.hh"

#include <array>

namespace openmsx {

class ReproCartridgeV2 final : public MSXRom
{
public:
	ReproCartridgeV2(DeviceConfig& config, Rom&& rom);
	~ReproCartridgeV2() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	void writeIO(uint16_t port, byte value, EmuTime time) override;

	void setVolume(EmuTime time, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isSCCAccess(uint16_t addr) const;
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
	std::array<byte, 4> bankRegs;
};

} // namespace openmsx

#endif
