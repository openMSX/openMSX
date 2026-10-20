#include "SdMapperV1.hh"

#include "CheckedRam.hh"
#include "SdCard.hh"

#include "CacheLine.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"

#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

namespace openmsx {

// Fabio Belavenuto's MSX SD Mapper v1 (msxsdmapper).
// Config type is SdMapperV1. SD Mapper v2 is a different, incompatible device.
//
// CPLD (CPLD/src/sdmapper.vhd, megamapper.vhd, sd.vhd):
// - 128kB flash. Write $6000 selects the 16kB bank shown at $4000-$7FFF.
//   $8000-$BFFF reads bank 0. Port $5F bit 7 allows flash writes and bits 2-0
//   select which 16kB page is mapped for those accesses.
// - Write $6001 bit 0 switches $4000-$47FF (data) and $4800 (status/CS)
//   between the flash and the SPI master. A data access completes one SPI
//   byte in the CPLD; the CPU reads the previous byte and the new result
//   is latched for the next access.
// - RAM switch on: standard slot expander, flash/SPI in subslot 1, the same
//   512kB SRAM in subslot 3. Mode switch selects memory mapper ($FC-$FF,
//   32 x 16kB) or MegaRAM ($8E/$8F, 64 x 8kB, A15 ignored).
// - RAM switch off: no expander, the whole slot is flash/SPI. The mode switch
//   still shows up in status bit 5.
// - Status bit 7 is SPI busy (sd.vhd: busy <= start). The 25MHz oscillator
//   drops it 19 clocks, about 0.76us, after a data access. A Z80, or an R800
//   fetching from an external slot, cannot read $4800 that soon. Code in
//   DRAM, or a faster CPU, can.

static constexpr unsigned RAM_SIZE = 512 * 1024;
static constexpr byte MAPPER_MASK = RAM_SIZE / 0x4000 - 1; // 16kB pages
static constexpr byte MEGA_MASK   = RAM_SIZE / 0x2000 - 1; // 8kB blocks
// 2 clocks to enter s_running, then 17 clocks until ff_clr on the 8th bit.
static constexpr auto SPI_BUSY_TIME = EmuDuration::hz(25e6) * 19;

[[nodiscard]] static bool isMapperMode(const DeviceConfig& config)
{
	auto mode = config.getChildData("mode", "mapper");
	if (mode == "mapper") return true;
	if (mode == "megaram") return false;
	throw MSXException("Unknown SD Mapper v1 mode \"", mode,
	                   "\". Expected \"mapper\" or \"megaram\".");
}

SdMapperV1::MapperIO::MapperIO(SdMapperV1& device_)
	: MSXMapperIOClient(device_.getMotherBoard())
	, device(device_)
{
}

byte SdMapperV1::MapperIO::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte SdMapperV1::MapperIO::peekIO(uint16_t port, EmuTime /*time*/) const
{
	// 512kB mapper readback: bits 7-5 are 1, bits 4-0 are the segment.
	return byte(~MAPPER_MASK | (device.banks[port & 3] & MAPPER_MASK));
}

void SdMapperV1::MapperIO::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	byte page = port & 3;
	// The register is 6 bits wide; mapper addressing uses the low 5.
	device.banks[page] = byte(value & MEGA_MASK);
	device.invalidateDeviceRWCache(0x4000 * page, 0x4000);
}

byte SdMapperV1::MapperIO::getSelectedSegment(byte page) const
{
	return byte(device.banks[page] & MAPPER_MASK);
}

SdMapperV1::SdMapperV1(DeviceConfig& config)
	: MSXDevice(config)
	, mapperMode(isMapperMode(config))
	, flash(getName() + " flash", AmdFlashChip::AM29F010, {}, config)
	, ram(config.getChildDataAsBool("ram", true)
		? std::make_unique<CheckedRam>(config, getName() + " RAM", "SD Mapper v1 RAM", RAM_SIZE)
		: nullptr)
	, mapperIO((ram && mapperMode) ? std::make_unique<MapperIO>(*this) : nullptr)
{
	sdCard[0] = std::make_unique<SdCard>(DeviceConfig(config, config.findChild("sdcard1")));
	sdCard[1] = std::make_unique<SdCard>(DeviceConfig(config, config.findChild("sdcard2")));

	powerUp(getCurrentTime());
	// $5F is always present. $8E/$8F exist only in MegaRAM mode, so they
	// cannot be listed unconditionally in the config file.
	getCPUInterface().register_IO_Out(0x5F, this);
	if (ram && !mapperMode) {
		getCPUInterface().register_IO_InOut(0x8E, this);
		getCPUInterface().register_IO_InOut(0x8F, this);
	}
}

SdMapperV1::~SdMapperV1()
{
	getCPUInterface().unregister_IO_Out(0x5F, this);
	if (ram && !mapperMode) {
		getCPUInterface().unregister_IO_InOut(0x8E, this);
		getCPUInterface().unregister_IO_InOut(0x8F, this);
	}
}

void SdMapperV1::powerUp(EmuTime time)
{
	if (ram) ram->clear();
	reset(time);
}

void SdMapperV1::reset(EmuTime time)
{
	subslotReg = 0;
	romBank = 0;
	sdEnabled = false;
	flashWriteEnable = false;
	flashPage = 0;
	cardCs = 0x03;
	spiIn = 0xFF;
	spiBusyUntil = time;
	megaWriteEnable = false;
	ranges::iota(banks, byte(0));
	flash.reset();
	invalidateDeviceRWCache();
}

byte SdMapperV1::getSubSlot(uint16_t addr) const
{
	if (!expanderEnabled()) return 1;
	return byte((subslotReg >> (2 * (addr >> 14))) & 3);
}

bool SdMapperV1::spiDataRegion(uint16_t addr) const
{
	return (0x4000 <= addr) && (addr < 0x4800);
}

bool SdMapperV1::spiMapped(uint16_t addr) const
{
	return sdEnabled && !flashWriteEnable && (spiDataRegion(addr) || (addr == 0x4800));
}

bool SdMapperV1::flashVisible(uint16_t addr) const
{
	auto page = addr >> 14;
	return (page == 1) || (page == 2);
}

unsigned SdMapperV1::flashAddress(uint16_t addr) const
{
	unsigned bank = flashWriteEnable ? flashPage
	              : ((addr >> 14) == 1 ? romBank : 0);
	return (bank << 14) | (addr & 0x3FFF);
}

unsigned SdMapperV1::ramAddress(uint16_t addr) const
{
	if (mapperMode) {
		unsigned segment = banks[addr >> 14] & MAPPER_MASK;
		return (segment << 14) | (addr & 0x3FFF);
	}
	unsigned block = banks[(addr >> 13) & 3] & MEGA_MASK;
	return (block << 13) | (addr & 0x1FFF);
}

byte SdMapperV1::readStatus(EmuTime time) const
{
	// bit 7: 1 while the shift started by the last data access is running.
	// bit 6: reserved, always 0.
	// bit 5: 1 = mapper, 0 = MegaRAM.
	// bit 4: 1 = RAM enabled.
	// bits 3-2: 1 = write protected.
	// bits 1-0: 0 = card present.
	byte value = 0;
	if (time < spiBusyUntil)           value |= 0x80;
	if (!sdCard[0]->isPresent())       value |= 0x01;
	if (!sdCard[1]->isPresent())       value |= 0x02;
	if (sdCard[0]->isWriteProtected()) value |= 0x04;
	if (sdCard[1]->isWriteProtected()) value |= 0x08;
	if (ram)                           value |= 0x10;
	if (mapperMode)                    value |= 0x20;
	return value;
}

byte SdMapperV1::transferSpi(byte value)
{
	// MISO is shared. A deselected card returns 0xFF and does not change state.
	byte result = 0xFF;
	for (auto i : xrange(2)) {
		bool csHigh = (cardCs & (1 << i)) != 0;
		result &= sdCard[i]->transfer(value, csHigh);
	}
	return result;
}

byte SdMapperV1::accessSpiData(EmuTime time, byte value)
{
	byte result = spiIn;
	spiIn = transferSpi(value);
	spiBusyUntil = time + SPI_BUSY_TIME;
	return result;
}

byte SdMapperV1::readFlashSlot(uint16_t addr, EmuTime time)
{
	if (spiMapped(addr)) {
		return spiDataRegion(addr) ? accessSpiData(time) : readStatus(time);
	}
	if (flashVisible(addr)) {
		return flash.read(flashAddress(addr), time);
	}
	return 0xFF;
}

byte SdMapperV1::peekFlashSlot(uint16_t addr, EmuTime time) const
{
	if (spiMapped(addr)) {
		return spiDataRegion(addr) ? spiIn : readStatus(time);
	}
	if (flashVisible(addr)) {
		return flash.peek(flashAddress(addr), time);
	}
	return 0xFF;
}

const byte* SdMapperV1::getFlashSlotReadCacheLine(uint16_t addr) const
{
	if (spiMapped(addr)) {
		return nullptr;
	}
	if (flashVisible(addr)) {
		return flash.getReadCacheLine(flashAddress(addr));
	}
	return unmappedRead.data();
}

void SdMapperV1::writeFlashSlot(uint16_t addr, byte value, EmuTime time)
{
	// $5F bit 7 inhibits the $6000/$6001 switches and the SPI window.
	// Flash /WE is generated only while SPI mode is off, so a write with
	// both enables set, including a write to $6001, is ignored.
	if (flashWriteEnable) {
		if (!sdEnabled) {
			flash.write(flashAddress(addr), value, time);
		}
		return;
	}
	if (addr == 0x6000) {
		byte bank = byte(value & 7);
		if (bank != romBank) {
			romBank = bank;
			invalidateDeviceRWCache(0x4000, 0x4000);
		}
		return;
	}
	if (addr == 0x6001) {
		bool enable = (value & 1) != 0;
		if (enable != sdEnabled) {
			sdEnabled = enable;
			invalidateDeviceRWCache(0x4000, 0x1000);
		}
		return;
	}
	if (sdEnabled) {
		if (spiDataRegion(addr)) {
			accessSpiData(time, value);
		} else if (addr == 0x4800) {
			cardCs = byte(value & 0x03);
		}
	}
}

byte SdMapperV1::readRamSlot(uint16_t addr)
{
	return ram->read(ramAddress(addr));
}

byte SdMapperV1::peekRamSlot(uint16_t addr) const
{
	return ram->peek(ramAddress(addr));
}

void SdMapperV1::writeRamSlot(uint16_t addr, byte value)
{
	if (mapperMode || megaWriteEnable) {
		ram->write(ramAddress(addr), value);
		return;
	}
	unsigned page = (addr >> 13) & 3;
	banks[page] = byte(value & MEGA_MASK);
	invalidateDeviceRWCache(page * 0x2000, 0x2000);
	invalidateDeviceRWCache(0x8000 + page * 0x2000, 0x2000);
}

byte SdMapperV1::peekMem(uint16_t addr, EmuTime time) const
{
	if (expanderEnabled() && addr == 0xFFFF) {
		return byte(subslotReg ^ 0xFF);
	}
	switch (getSubSlot(addr)) {
	case 1: return peekFlashSlot(addr, time);
	case 3: return peekRamSlot(addr);
	default: return 0xFF;
	}
}

byte SdMapperV1::readMem(uint16_t addr, EmuTime time)
{
	if (expanderEnabled() && addr == 0xFFFF) {
		return byte(subslotReg ^ 0xFF);
	}
	switch (getSubSlot(addr)) {
	case 1: return readFlashSlot(addr, time);
	case 3: return readRamSlot(addr);
	default: return 0xFF;
	}
}

const byte* SdMapperV1::getReadCacheLine(uint16_t addr) const
{
	if (expanderEnabled() &&
	    ((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		return nullptr;
	}
	switch (getSubSlot(addr)) {
	case 1: return getFlashSlotReadCacheLine(addr);
	case 3: return ram->getReadCacheLine(ramAddress(addr));
	default: return unmappedRead.data();
	}
}

void SdMapperV1::writeMem(uint16_t addr, byte value, EmuTime time)
{
	if (expanderEnabled() && addr == 0xFFFF) {
		byte diff = byte(value ^ subslotReg);
		subslotReg = value;
		for (auto page : xrange(4)) {
			if (diff & (3 << (2 * page))) {
				invalidateDeviceRWCache(0x4000 * page, 0x4000);
			}
		}
		return;
	}
	switch (getSubSlot(addr)) {
	case 1: writeFlashSlot(addr, value, time); break;
	case 3: writeRamSlot(addr, value); break;
	default: break;
	}
}

byte* SdMapperV1::getWriteCacheLine(uint16_t addr)
{
	if (expanderEnabled() &&
	    ((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		return nullptr;
	}
	switch (getSubSlot(addr)) {
	case 1:
		return nullptr;
	case 3:
		if (!mapperMode && !megaWriteEnable) return nullptr;
		return ram->getWriteCacheLine(ramAddress(addr));
	default:
		return unmappedWrite.data();
	}
}

byte SdMapperV1::readIO(uint16_t port, EmuTime /*time*/)
{
	// $5F is registered for output only. Input is only $8E/$8F, and both
	// ports have the same effect (megamapper.vhd io8x).
	switch (port & 0xFF) {
	case 0x8E:
	case 0x8F:
		// IN enables SRAM writes and disables bank switching.
		if (!megaWriteEnable) {
			megaWriteEnable = true;
			invalidateDeviceRWCache();
		}
		return 0xFF;
	default:
		UNREACHABLE;
		return 0xFF;
	}
}

byte SdMapperV1::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return 0xFF;
}

void SdMapperV1::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	switch (port & 0xFF) {
	case 0x5F:
		flashWriteEnable = (value & 0x80) != 0;
		flashPage = byte(value & 7);
		invalidateDeviceRWCache();
		break;
	case 0x8E:
	case 0x8F:
		// OUT $8E/$8F disables SRAM writes and enables bank switching.
		if (megaWriteEnable) {
			megaWriteEnable = false;
			invalidateDeviceRWCache();
		}
		break;
	default:
		UNREACHABLE;
	}
}

template<typename Archive>
void SdMapperV1::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash",            flash,
	             "subslotReg",       subslotReg,
	             "romBank",          romBank,
	             "sdEnabled",        sdEnabled,
	             "flashWriteEnable", flashWriteEnable,
	             "flashPage",        flashPage,
	             "cardCs",           cardCs,
	             "spiIn",            spiIn,
	             "spiBusyUntil",     spiBusyUntil,
	             "banks",            banks,
	             "megaWriteEnable",  megaWriteEnable,
	             "sdCard0",          *sdCard[0],
	             "sdCard1",          *sdCard[1]);
	if (ram) ar.serialize("ram", ram->getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(SdMapperV1);
REGISTER_MSXDEVICE(SdMapperV1, "SdMapperV1");

} // namespace openmsx
