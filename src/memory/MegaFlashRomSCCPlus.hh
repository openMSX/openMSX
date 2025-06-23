#ifndef MEGAFLASHROMSCCPLUS_HH
#define MEGAFLASHROMSCCPLUS_HH

#include "MSXRom.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include "AmdFlash.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class MegaFlashRomSCCPlus final : public MSXRom
{
public:
	MegaFlashRomSCCPlus(DeviceConfig& config, Rom&& rom);
	~MegaFlashRomSCCPlus() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] byte readMem2(uint16_t addr, EmuTime::param time);

	enum class SCCMode { NONE, SCC, SCCPLUS };
	[[nodiscard]] SCCMode getSCCMode() const;

	[[nodiscard]] unsigned getSubslot(unsigned address) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;
	[[nodiscard]] bool isSCCAccessEnabled() const;

private:
	SCC scc;
	AY8910 psg;
	AmdFlash flash;

	byte configReg;
	byte offsetReg;
	byte subslotReg;
	std::array<std::array<byte, 4>, 4> bankRegs;
	byte psgLatch;
	byte sccModeReg;
	std::array<byte, 4> sccBanks;
};

} // namespace openmsx

#endif
