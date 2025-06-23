#ifndef REPROCARTRIDGEV1_HH
#define REPROCARTRIDGEV1_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include <array>

namespace openmsx {

class ReproCartridgeV1 final : public MSXRom
{
public:
	ReproCartridgeV1(DeviceConfig& config, Rom&& rom);
	~ReproCartridgeV1() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	void setVolume(EmuTime::param time, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isSCCAccess(uint16_t addr) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

private:
	AmdFlash flash;
	SCC scc;
	AY8910 psg;

	bool flashRomWriteEnabled;
	byte mainBankReg;
	byte psgLatch;
	byte sccMode;
	std::array<byte, 4> bankRegs;
};

} // namespace openmsx

#endif
