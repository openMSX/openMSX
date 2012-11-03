// $Id$

#ifndef MEGAFLASHROMSCCPLUS_HH
#define MEGAFLASHROMSCCPLUS_HH

#include "MSXRom.hh"
#include <memory>

namespace openmsx {

class SCC;
class AY8910;
class AmdFlash;

class MegaFlashRomSCCPlus : public MSXRom
{
public:
	MegaFlashRomSCCPlus(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~MegaFlashRomSCCPlus();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

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
	const std::unique_ptr<AmdFlash> flash;

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
