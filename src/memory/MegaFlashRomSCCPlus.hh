#ifndef MEGAFLASHROMSCCPLUS_HH
#define MEGAFLASHROMSCCPLUS_HH

#include "MSXRom.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include "AmdFlash.hh"
#include <array>

namespace openmsx {

class MegaFlashRomSCCPlus final : public MSXRom
{
public:
	MegaFlashRomSCCPlus(const DeviceConfig& config, Rom&& rom);
	~MegaFlashRomSCCPlus() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] byte readMem2(word addr, EmuTime::param time);

	enum SCCEnable { EN_NONE, EN_SCC, EN_SCCPLUS };
	[[nodiscard]] SCCEnable getSCCEnable() const;

	[[nodiscard]] unsigned getSubslot(unsigned address) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

private:
	SCC scc;
	AY8910 psg;
	AmdFlash flash;

	byte configReg;
	byte offsetReg;
	byte subslotReg;
	std::array<std::array<byte, 4>, 4> bankRegs;
	byte psgLatch;
	byte sccMode;
	std::array<byte, 4> sccBanks;
};

} // namespace openmsx

#endif
