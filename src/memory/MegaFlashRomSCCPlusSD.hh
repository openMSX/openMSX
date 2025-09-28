#ifndef MEGAFLASHROMSCCPLUSSD_HH
#define MEGAFLASHROMSCCPLUSSD_HH

#include "AmdFlash.hh"
#include "MSXMapperIO.hh"

#include "AY8910.hh"
#include "MSXDevice.hh"
#include "SCC.hh"

#include <array>
#include <cstdint>
#include <memory>

namespace openmsx {

class CheckedRam;
class SdCard;

class MegaFlashRomSCCPlusSD final : public MSXDevice
{
public:
	explicit MegaFlashRomSCCPlusSD(DeviceConfig& config);
	~MegaFlashRomSCCPlusSD() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	enum SCCEnable : uint8_t { EN_NONE, EN_SCC, EN_SCCPLUS };
	[[nodiscard]] SCCEnable getSCCEnable() const;
	void updateConfigReg(byte value);

	[[nodiscard]] byte getSubSlot(unsigned addr) const;

	/**
	 * Writes to flash only if it was not write protected.
	 */
	void writeToFlash(unsigned addr, byte value, EmuTime time);
	AmdFlash flash;
	byte subslotReg;

	// subslot 0
	[[nodiscard]] byte readMemSubSlot0(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekMemSubSlot0(uint16_t address, EmuTime time) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot0(uint16_t address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot0(uint16_t address);
	void writeMemSubSlot0(uint16_t address, byte value, EmuTime time);

	// subslot 1
	// mega flash rom scc+
	[[nodiscard]] byte readMemSubSlot1(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekMemSubSlot1(uint16_t address, EmuTime time) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot1(uint16_t address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot1(uint16_t address);
	void writeMemSubSlot1(uint16_t address, byte value, EmuTime time);
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
	[[nodiscard]] byte readMemSubSlot2(uint16_t address);
	[[nodiscard]] byte peekMemSubSlot2(uint16_t address) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot2(uint16_t address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot2(uint16_t address);
	void writeMemSubSlot2(uint16_t address, byte value);

	[[nodiscard]] unsigned calcMemMapperAddress(uint16_t address) const;
	[[nodiscard]] unsigned calcAddress(uint16_t address) const;

	class MapperIO final : public MSXMapperIOClient {
	public:
		explicit MapperIO(MegaFlashRomSCCPlusSD& mega_)
			: MSXMapperIOClient(mega_.getMotherBoard())
			, mega(mega_)
		{
		}

		[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
		[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
		void writeIO(uint16_t port, byte value, EmuTime time) override;
		[[nodiscard]] byte getSelectedSegment(byte page) const override;

	private:
		MegaFlashRomSCCPlusSD& mega;
	};
	const std::unique_ptr<CheckedRam> checkedRam; // can be nullptr
	const std::unique_ptr<MapperIO> mapperIO; // nullptr iff checkedRam == nullptr
	std::array<byte, 4> memMapperRegs;

	// subslot 3
	[[nodiscard]] byte readMemSubSlot3(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekMemSubSlot3(uint16_t address, EmuTime time) const;
	[[nodiscard]] const byte* getReadCacheLineSubSlot3(uint16_t address) const;
	[[nodiscard]] byte* getWriteCacheLineSubSlot3(uint16_t address);
	void writeMemSubSlot3(uint16_t address, byte value, EmuTime time);
	[[nodiscard]] unsigned getFlashAddrSubSlot3(unsigned addr) const;

	std::array<byte, 4> bankRegsSubSlot3;

	byte selectedCard;
	std::array<std::unique_ptr<SdCard>, 2> sdCard;
};

} // namespace openmsx

#endif
