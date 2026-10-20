#ifndef SDMAPPERV1_HH
#define SDMAPPERV1_HH

#include "AmdFlash.hh"
#include "MSXDevice.hh"
#include "MSXMapperIO.hh"

#include <array>
#include <memory>

namespace openmsx {

class CheckedRam;
class SdCard;

class SdMapperV1 final : public MSXDevice
{
public:
	explicit SdMapperV1(DeviceConfig& config);
	~SdMapperV1() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool expanderEnabled() const { return ram != nullptr; }
	[[nodiscard]] byte getSubSlot(uint16_t addr) const;
	[[nodiscard]] bool spiMapped(uint16_t addr) const;
	[[nodiscard]] bool spiDataRegion(uint16_t addr) const;
	[[nodiscard]] bool flashVisible(uint16_t addr) const;
	[[nodiscard]] unsigned flashAddress(uint16_t addr) const;
	[[nodiscard]] unsigned ramAddress(uint16_t addr) const;
	[[nodiscard]] byte readStatus(EmuTime time) const;
	[[nodiscard]] byte transferSpi(byte value);
	byte accessSpiData(EmuTime time, byte value = 0xFF);

	[[nodiscard]] byte readFlashSlot(uint16_t addr, EmuTime time);
	[[nodiscard]] byte peekFlashSlot(uint16_t addr, EmuTime time) const;
	[[nodiscard]] const byte* getFlashSlotReadCacheLine(uint16_t addr) const;
	void writeFlashSlot(uint16_t addr, byte value, EmuTime time);

	[[nodiscard]] byte readRamSlot(uint16_t addr);
	[[nodiscard]] byte peekRamSlot(uint16_t addr) const;
	void writeRamSlot(uint16_t addr, byte value);

	class MapperIO final : public MSXMapperIOClient {
	public:
		explicit MapperIO(SdMapperV1& device_);

		[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
		[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
		void writeIO(uint16_t port, byte value, EmuTime time) override;
		[[nodiscard]] byte getSelectedSegment(byte page) const override;

	private:
		SdMapperV1& device;
	};

	// mr_mp switch. Status bit 5 reports it even when RAM is disabled,
	// so this is not the same as `mapperIO != nullptr`.
	const bool mapperMode;
	AmdFlash flash;
	const std::unique_ptr<CheckedRam> ram;       // nullptr when RAM is disabled
	const std::unique_ptr<MapperIO> mapperIO;    // only in mapper mode
	std::array<std::unique_ptr<SdCard>, 2> sdCard;

	byte subslotReg = 0;
	byte romBank = 0;             // ASCII16 bank for page 1, 3 bits
	bool sdEnabled = false;       // $6001 bit 0
	bool flashWriteEnable = false;
	byte flashPage = 0;           // port $5F bits 2-0
	byte cardCs = 0x03;           // 1 = /CS high (deselected)
	byte spiIn = 0xFF;            // result of the previous SPI byte
	EmuTime spiBusyUntil = EmuTime::zero(); // status bit 7
	std::array<byte, 4> banks = {};
	bool megaWriteEnable = false;
};

} // namespace openmsx

#endif
