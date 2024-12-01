#ifndef MEGAFLASHROMSCCPLUSSD_HH
#define MEGAFLASHROMSCCPLUSSD_HH

#include "MSXDevice.hh"
#include "MSXMapperIO.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include <array>
#include <memory>

namespace openmsx {

class CheckedRam;
class SdCard;

class MegaFlashRomSCCPlusSD final : public MSXDevice
{
public:
	explicit MegaFlashRomSCCPlusSD(const DeviceConfig& config);
	~MegaFlashRomSCCPlusSD() override;

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
	enum SCCEnable { EN_NONE, EN_SCC, EN_SCCPLUS };
	[[nodiscard]] SCCEnable getSCCEnable() const;
	void updateConfigReg(byte value);

	[[nodiscard]] byte getSubSlot(unsigned addr) const;

	/**
	 * Writes to flash only if it was not write protected.
	 */
	void writeToFlash(unsigned addr, byte value);
	AmdFlash flash;
	byte subslotReg;

	// subslot 0
	[[nodiscard]] byte readMemSubSlot0(word address);
	[[nodiscard]] byte peekMemSubSlot0(word address) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot0(word address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot0(word address);
	void writeMemSubSlot0(word address, byte value);

	// subslot 1
	// mega flash rom scc+
	[[nodiscard]] byte readMemSubSlot1(word address, EmuTime::param time);
	[[nodiscard]] byte peekMemSubSlot1(word address, EmuTime::param time) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot1(word address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot1(word address);
	void writeMemSubSlot1(word address, byte value, EmuTime::param time);
	[[nodiscard]] unsigned getFlashAddrSubSlot1(unsigned addr) const;

	SCC scc;
	AY8910 psg;

	byte mapperReg;
	[[nodiscard]] bool is64KmapperConfigured()               const { return (mapperReg & 0xC0) == 0x40; }
	[[nodiscard]] bool isKonamiSCCmapperConfigured()         const { return (mapperReg & 0xE0) == 0x00; }
	[[nodiscard]] bool isWritingKonamiBankRegisterDisabled() const { return (mapperReg & 0x08) != 0; }
	[[nodiscard]] bool isMapperRegisterDisabled()            const { return (mapperReg & 0x04) != 0; }
	[[nodiscard]] bool areBankRegsAndOffsetRegsDisabled()    const { return (mapperReg & 0x02) != 0; }
	[[nodiscard]] bool areKonamiMapperLimitsEnabled()        const { return (mapperReg & 0x01) != 0; }
	unsigned offsetReg;

	byte configReg = 3; // avoid UMR
	[[nodiscard]] bool isConfigRegDisabled()           const { return  (configReg & 0x80) != 0; }
	[[nodiscard]] bool isMemoryMapperEnabled()         const { return ((configReg & 0x20) == 0) && checkedRam; }
	[[nodiscard]] bool isDSKmodeEnabled()              const { return  (configReg & 0x10) != 0; }
	[[nodiscard]] bool isPSGalsoMappedToNormalPorts()  const { return  (configReg & 0x08) != 0; }
	[[nodiscard]] bool isSlotExpanderEnabled()         const { return  (configReg & 0x04) == 0; }
	[[nodiscard]] bool isFlashRomBlockProtectEnabled() const { return  (configReg & 0x02) != 0; }
	[[nodiscard]] bool isFlashRomWriteEnabled()        const { return  (configReg & 0x01) != 0; }

	// Note: the bankRegsSubSlot1 registers are actually only 9 bit.
	std::array<uint16_t, 4> bankRegsSubSlot1;
	byte psgLatch;
	byte sccMode;
	std::array<byte, 4> sccBanks;

	// subslot 2
	// 512k memory mapper
	[[nodiscard]] byte readMemSubSlot2(word address);
	[[nodiscard]] byte peekMemSubSlot2(word address) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot2(word address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot2(word address);
	void writeMemSubSlot2(word address, byte value);

	[[nodiscard]] unsigned calcMemMapperAddress(word address) const;
	[[nodiscard]] unsigned calcAddress(word address) const;

	class MapperIO final : public MSXMapperIOClient {
	public:
		explicit MapperIO(MegaFlashRomSCCPlusSD& mega_)
			: MSXMapperIOClient(mega_.getMotherBoard())
			, mega(mega_)
		{
		}

		[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
		[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
		void writeIO(word port, byte value, EmuTime::param time) override;
		[[nodiscard]] byte getSelectedSegment(byte page) const override;

	private:
		MegaFlashRomSCCPlusSD& mega;
	};
	const std::unique_ptr<CheckedRam> checkedRam; // can be nullptr
	const std::unique_ptr<MapperIO> mapperIO; // nullptr iff checkedRam == nullptr
	std::array<byte, 4> memMapperRegs;

	// subslot 3
	[[nodiscard]] byte readMemSubSlot3(word address, EmuTime::param time);
	[[nodiscard]] byte peekMemSubSlot3(word address, EmuTime::param time) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot3(word address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot3(word address);
	void writeMemSubSlot3(word address, byte value, EmuTime::param time);
	[[nodiscard]] unsigned getFlashAddrSubSlot3(unsigned addr) const;

	std::array<byte, 4> bankRegsSubSlot3;

	byte selectedCard;
	std::array<std::unique_ptr<SdCard>, 2> sdCard;
};

} // namespace openmsx

#endif
