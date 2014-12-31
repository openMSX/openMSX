#ifndef MEGAFLASHROMSCCPLUS_HH
#define MEGAFLASHROMSCCPLUS_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include <memory>

namespace openmsx {

class SCC;
class AY8910;

class MegaFlashRomSCCPlus final : public MSXRom
{
public:
	MegaFlashRomSCCPlus(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~MegaFlashRomSCCPlus();

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readMem2(word addr, EmuTime::param time);

	enum SCCEnable { EN_NONE, EN_SCC, EN_SCCPLUS };
	SCCEnable getSCCEnable() const;

	unsigned getSubslot(unsigned address) const;
	unsigned getFlashAddr(unsigned addr) const;

	const std::unique_ptr<SCC> scc;
	const std::unique_ptr<AY8910> psg;
	AmdFlash flash;

	byte configReg;
	byte offsetReg;
	byte subslotReg;
	byte bankRegs[4][4];
	byte psgLatch;
	byte sccMode;
	byte sccBanks[4];
};

} // namespace openmsx

#endif
