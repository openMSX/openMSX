#ifndef MEGAFLASHROMSCCPLUSSD_HH
#define MEGAFLASHROMSCCPLUSSD_HH

#include "MSXRom.hh"
#include <memory>

namespace openmsx {

class SCC;
class AY8910;
class AmdFlash;
class CheckedRam;
class SdCard;

class MegaFlashRomSCCPlusSD final : public MSXRom
{
public:
	MegaFlashRomSCCPlusSD(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~MegaFlashRomSCCPlusSD();

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
	enum SCCEnable { EN_NONE, EN_SCC, EN_SCCPLUS };
	SCCEnable getSCCEnable() const;
	void updateConfigReg(byte value);

	byte getSubSlot(unsigned addr) const;

	/**
	 * Writes to flash only if it was not write protected.
	 */
	void writeToFlash(unsigned addr, byte value);
	const std::unique_ptr<AmdFlash> flash;
	byte subslotReg;

	// subslot 0
	byte readMemSubSlot0(word address);
	byte peekMemSubSlot0(word address) const;
	const byte* getReadCacheLineSubSlot0(word address) const;
	byte* getWriteCacheLineSubSlot0(word address) const;
	void writeMemSubSlot0(word address, byte value);

	// subslot 1
	// mega flash rom scc+
	byte readMemSubSlot1(word address, EmuTime::param time);
	byte peekMemSubSlot1(word address, EmuTime::param time) const;
	const byte* getReadCacheLineSubSlot1(word address) const;
	byte* getWriteCacheLineSubSlot1(word address) const;
	void writeMemSubSlot1(word address, byte value, EmuTime::param time);
	unsigned getFlashAddrSubSlot1(unsigned addr) const;

	const std::unique_ptr<SCC> scc;
	const std::unique_ptr<AY8910> psg;

	byte mapperReg;
	bool is64KmapperConfigured()               const { return (mapperReg & 0xC0) == 0x40; }
	bool isKonamiSCCmapperConfigured()         const { return (mapperReg & 0xE0) == 0x00; }
	bool isWritingKonamiBankRegisterDisabled() const { return mapperReg & 0x08; }
	bool isMapperRegisterDisabled()            const { return mapperReg & 0x04; }
	bool areBankRegsAndOffsetRegsDisabled()    const { return mapperReg & 0x02; }
	bool areKonamiMapperLimitsEnabled()        const { return mapperReg & 0x01; }
	unsigned offsetReg;

	byte configReg;
	bool isConfigRegDisabled()           const { return   configReg & 0x80;  }
	bool isMemoryMapperEnabled()         const { return !(configReg & 0x20) && checkedRam; }
	bool isDSKmodeEnabled()              const { return   configReg & 0x10;  }
	bool isPSGalsoMappedToNormalPorts()  const { return   configReg & 0x08;  }
	bool isSlotExpanderEnabled()         const { return !(configReg & 0x04); }
	bool isFlashRomBlockProtectEnabled() const { return   configReg & 0x02;  }
	bool isFlashRomWriteEnabled()        const { return   configReg & 0x01;  }

	byte bankRegsSubSlot1[4];
	byte psgLatch;
	byte sccMode;
	byte sccBanks[4];

	// subslot 2
	// 512k memory mapper
	byte readMemSubSlot2(word address);
	byte peekMemSubSlot2(word address) const;
	const byte* getReadCacheLineSubSlot2(word address) const;
	byte* getWriteCacheLineSubSlot2(word address) const;
	void writeMemSubSlot2(word address, byte value);

	unsigned calcMemMapperAddress(word address) const;
	unsigned calcAddress(word address) const;

	const std::unique_ptr<CheckedRam> checkedRam;
	byte memMapperRegs[4];

	// subslot 3
	byte readMemSubSlot3(word address, EmuTime::param time);
	byte peekMemSubSlot3(word address, EmuTime::param time) const;
	const byte* getReadCacheLineSubSlot3(word address) const;
	byte* getWriteCacheLineSubSlot3(word address) const;
	void writeMemSubSlot3(word address, byte value, EmuTime::param time);
	unsigned getFlashAddrSubSlot3(unsigned addr) const;

	byte bankRegsSubSlot3[4];

	byte selectedCard;
	std::unique_ptr<SdCard> sdCard[2];
};

} // namespace openmsx

#endif
