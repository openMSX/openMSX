// Y8960 mapper
//
// memory mapped I/O
//    The memory mapped registers below are the cartridge's way in to the
//    sound blocks.
//
//    The window 0x7FE0-0x7FFF is fully covered by the SCC window of the same
//    page (0x7800-0x7FFF), so it is only there while 0x6000-0x7FFF shows
//    neither RAM nor the SCC registers.
//
//    Always-valid register:
//  	RamEnable:  0x48FB, 0x49FB, 0x4AFB, 0x4BFB, 0x4CFB, 0x4DFB, 0x4EFB, 0x4FFB
//					Enable RAM write control
//
//    The window's addresses and the enabler bits are the constants right
//    below this comment. They are still subject to change on the hardware
//    side, so they are kept in that one place.
//
//  	OPLL:       tunnel to the OPLL I/O ports (address / data)
//  	OPL2:       tunnel to the OPL2 I/O ports (address / data)
//  	SSGS:       tunnel to the SSGS I/O ports (address / data)
//  	OPM:        tunnel to the OPM I/O ports (address / data)
//
//  	I/O Enabler1: open the OPLL I/O ports (0x7C-0x7D)
//  	I/O Enabler2: open the OPL2 (0xC0-0xC1), SSGS (0xA0-0xA2), OPM
//					(0x7E-0x7F) and MSX-TIMER (0xB0-0xB3) I/O ports
//					All are closed after reset, like the FM-PAC. The tunnels
//					above stay open, otherwise there is no way to open them.
//					Readable: the bits without a block read as 0.
//
//  	Volume:		the mixer's volume per block (SSG, OPLL, OPL2, SCC, OPM)
//					and its master volume. The mixer has no other way in.
//
//  	SCC:		0x1800-0x1FFF of any bank whose bank register holds #63,
//  				i.e. 0x5800-0x5FFF / 0x7800-0x7FFF / 0x9800-0x9FFF /
//  				0xB800-0xBFFF. Only 0x9800-0x9FFF is Konami SCC compatible.
//					In RAM mode 0x4000-0x5FFF is excluded: control page.
//					SCC sound register, decoded on the low 8 address bits
//  	SCC pan:	0x9900-0x9904, channel A-E, write only, in the Konami SCC
//					compatible window only. Writes there never reach the SCC.
//					Reads, and the rest of 0x9900-0x99FF, go to the SCC.
//
//    Registers available when RamEnable is 0:
//  	bank 1: 	0x5000 - 0x57ff
//					Bank switch for 0x4000-0x5FFF
//
//  	bank 2: 	0x7000 - 0x77ff
//					Bank switch for 0x6000-0x7FFF
//
//  	bank 3: 	0x9000 - 0x97ff
//					Bank switch for 0x8000-0x9FFF
//
//  	bank 4: 	0xB000 - 0xB7ff
//					Bank switch for 0xA000-0xBFFF
//
//    Registers available when RamEnable is 1:
//  	bank 1: 	0x48FC, 0x49FC, 0x4AFC, 0x4BFC, 0x4CFC, 0x4DFC, 0x4EFC, 0x4FFC
//					Bank switch for 0x4000-0x5FFF
//
//  	bank 2: 	0x48FD, 0x49FD, 0x4AFD, 0x4BFD, 0x4CFD, 0x4DFD, 0x4EFD, 0x4FFD
//					Bank switch for 0x6000-0x7FFF
//
//  	bank 3: 	0x48FE, 0x49FE, 0x4AFE, 0x4BFE, 0x4CFE, 0x4DFE, 0x4EFE, 0x4FFE
//					Bank switch for 0x8000-0x9FFF
//
//  	bank 4: 	0x48FF, 0x49FF, 0x4AFF, 0x4BFF, 0x4CFF, 0x4DFF, 0x4EFF, 0x4FFF
//					Bank switch for 0xA000-0xBFFF
//
//    In RAM mode 0x4000-0x5FFF is not writable: writes there only reach the
//    registers above. Reads show whatever its bank register selected.
//
// bank (the hardware keeps only the low 6 bits of a bank register)
//		 0 - 15		ROM bank
//		16 - 31		RAM bank. Read-only while RamEnable is 0, like every bank
//					of the SCC mapper this one extends
//		32 - 62		mirror of 0 - 30: bit 4 alone tells ROM from RAM
//		63			SCC sound register

#include "RomY8960.hh"

#include "CacheLine.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "xrange.hh"

namespace openmsx {

namespace {

// The memory mapped I/O window. Everything in it is write-only, except
// I/O Enabler2.
constexpr uint16_t MmioWindow       = 0x7FE0; // 0x7FE0-0x7FFF
constexpr uint16_t MmioSsgs         = 0x7FF0; // + 0x7FF1
constexpr uint16_t MmioOpl2         = 0x7FF2; // + 0x7FF3
constexpr uint16_t MmioOpll         = 0x7FF4; // + 0x7FF5
constexpr uint16_t MmioIoEnabler1   = 0x7FF6;
constexpr uint16_t MmioOpmAddress   = 0x7FFB;
constexpr uint16_t MmioOpmData      = 0x7FFC;
constexpr uint16_t MmioIoEnabler2   = 0x7FFF;
// + Y8960Mixer channel: 0x7FF7-0x7FFA and 0x7FFD. The OPM's ports sit in
// between, at channels 4 and 5.
constexpr uint16_t MmioVolume       = 0x7FF7;
constexpr uint16_t MmioMasterVolume = 0x7FFE;

constexpr byte Enabler1Opll  = 0x01;
constexpr byte Enabler2Opl2  = 0x01;
constexpr byte Enabler2Ssgs  = 0x02;
constexpr byte Enabler2Opm   = 0x04;
constexpr byte Enabler2Timer = 0x80;

template<typename T>
[[nodiscard]] T* findWiredDevice(MSXMotherBoard& motherBoard,
                                 const DeviceConfig& config, std::string_view element)
{
	auto devName = config.getChildData(element, "");
	if (devName.empty()) return nullptr;
	auto* device = dynamic_cast<T*>(motherBoard.findDevice(devName));
	if (device == nullptr) {
		motherBoard.getMSXCliComm().printWarning("can not found device '", devName, "'.");
	}
	return device;
}

} // anonymous namespace

RomY8960::RomY8960(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, ram(config, getName() + " ram", "ram", 8192 * RamBankCounts)
	, scc(getName(), config, getCurrentTime())
{
	// warn if a ROM is used that would not work on a real KonamiSCC mapper
	if (rom.size() > 8192 * RomBankCounts) {
		getMotherBoard().getMSXCliComm().printWarning(
			"The size of this ROM image is larger than ", 8 * RomBankCounts, "kB, "
			"which is not supported on real Konami SCC mapper "
			"chips!");
	}

	auto& motherBoard = getMotherBoard();
	opll  = findWiredDevice<Y8960OPLLDevice>(motherBoard, config, "opll");
	opl2  = findWiredDevice<Y8960OPL2Device>(motherBoard, config, "opl2");
	ssgs  = findWiredDevice<Y8960SSGSDevice>(motherBoard, config, "ssgs");
	opm   = findWiredDevice<Y8960OPMDevice> (motherBoard, config, "opm");
	timer = findWiredDevice<MSXTimer>       (motherBoard, config, "timer");
	mixerName = config.getChildData("mixer", "");

	powerUp(getCurrentTime());
}

void RomY8960::powerUp(EmuTime time)
{
	scc.powerUp(time);
	reset(time);
}

void RomY8960::bankSwitch(unsigned page, unsigned block)
{
	block %= RomBankCounts;
	setRom(page, block);

	// Note: the mirror behavior is different from RomKonami !
	if (page == 2 || page == 3) {
		// [0x4000-0x8000), mirrored in [0xC000-0x10000)
		setRom(page + 4, block);
	} else if (page == 4 || page == 5) {
		// [0x8000-0xC000), mirrored in [0x0000-0x4000)
		setRom(page - 4, block);
	} else {
		assert(false);
	}
}

bool RomY8960::isRamRegion(unsigned int region) const
{
	uint8_t bank = getBank(region);
	// SccBank has bit 4 set like a RAM bank, but it is not memory.
	if ((bank & 0x3F) == SccBank) return false;
	return (bank % (RomBankCounts + RamBankCounts)) >= RomBankCounts;
}

bool RomY8960::isSccRegion(unsigned int region) const
{
	// In RAM mode 0x4000-0x5FFF is the control page: the mode register and the
	// bank registers live there, so the SCC window is not offered on top.
	if (ramEnabled && (region == 2)) return false;
	return (getBank(region) & 0x3F) == SccBank;
}

bool RomY8960::isSccAddress(uint16_t address) const
{
	if ((address < 0x4000) || (address >= 0xC000)) return false;
	return ((address & 0x1800) == 0x1800) &&
	       isSccRegion(convAddressToRegion(address));
}

bool RomY8960::isSccPanAddress(uint16_t address)
{
	return (0x9900 <= address) && (address <= 0x9904);
}

bool RomY8960::isMmioVisible() const
{
	// The window 0x7FE0-0x7FFF lies entirely inside the SCC window of the same
	// page (0x7800-0x7FFF), so the two never share it: the window exists only
	// while 0x6000-0x7FFF shows neither RAM nor SCC.
	constexpr unsigned int MmioRegion = 3;
	if (ramEnabled && isRamRegion(MmioRegion)) return false;
	return !isSccRegion(MmioRegion);
}

uint8_t RomY8960::getBank(unsigned int region) const
{
	return bankReg[(region - 2) & 3];
}

void RomY8960::setBank(unsigned int region, uint8_t bank)
{
	bankReg[(region - 2) & 3] = bank;
}

const unsigned int RomY8960::getRamAddress(uint16_t address) const
{
	uint8_t bank = (getBank(convAddressToRegion(address)) % (RomBankCounts + RamBankCounts)) - RomBankCounts;
	assert(bank < RamBankCounts);
	return (bank * 0x2000) + (address & 0x1FFF);
}

unsigned int RomY8960::convAddressToRegion(uint16_t address) const
{
	return address >> 13;
}

void RomY8960::reset(EmuTime time)
{
	for (auto i : xrange(2, 6)) {
		bankSwitch(i, i - 2);
		setBank(i, i - 2);
	}

	ramEnabled = false;
	scc.reset(time);
}

bool RomY8960::isIoEnabler2Address(uint16_t address) const
{
	return (address == MmioIoEnabler2) && isMmioVisible();
}

byte RomY8960::peekIoEnabler2() const
{
	// The enable bits live in the blocks, which keep them across a savestate.
	byte value = 0;
	if ((opl2  != nullptr) && opl2 ->isIoEnabled()) value |= Enabler2Opl2;
	if ((ssgs  != nullptr) && ssgs ->isIoEnabled()) value |= Enabler2Ssgs;
	if ((opm   != nullptr) && opm  ->isIoEnabled()) value |= Enabler2Opm;
	if ((timer != nullptr) && timer->isIoEnabled()) value |= Enabler2Timer;
	return value;
}

byte RomY8960::peekMem(uint16_t address, EmuTime time) const
{
	if (isIoEnabler2Address(address)) {
		return peekIoEnabler2();
	} else if (isSccAddress(address)) {
		return scc.peekMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else if (isRamRegion(convAddressToRegion(address))) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

byte RomY8960::readMem(uint16_t address, EmuTime time)
{
	if (isIoEnabler2Address(address)) {
		return peekIoEnabler2();
	} else if (isSccAddress(address)) {
		return scc.readMem(narrow_cast<uint8_t>(address & 0xFF), time);
	} else if (isRamRegion(convAddressToRegion(address))) {
		return ram[getRamAddress(address)];
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* RomY8960::getReadCacheLine(uint16_t address) const
{
	if (isIoEnabler2Address(address | CacheLine::LOW)) {
		// I/O Enabler2 (0x7FFF) is the last byte of this line
		return nullptr;
	} else if (isSccAddress(address)) {
		// don't cache SCC
		return nullptr;
	} else if (isRamRegion(convAddressToRegion(address))) {
		// read from ram
		return &ram[getRamAddress(address)];
	} else {
		// read from rom
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomY8960::writeMem(uint16_t address, byte value, EmuTime time)
{
	if ((address < 0x4800) || (address >= 0xC000)) {
		return;
	}

	// write to SCC
	if (isSccAddress(address)) {
		if (isSccPanAddress(address)) {
			// The SCC decodes only the low 8 address bits and would take
			// these for 0x9800-0x9804. The cartridge's RTL of 2026-10-19
			// drops these writes in RAM mode; that is taken as a hardware
			// bug, since the firmware runs in RAM mode, so here they reach
			// the pan in both modes.
			scc.setPan(address - 0x9900, value, time);
		} else {
			scc.writeMem(narrow_cast<uint8_t>(address & 0xFF), value, time);
		}
		return;
	}

	// write to RAM(0x4000-0x5FFF is write-protected)
	if (ramEnabled && isRamRegion(convAddressToRegion(address)) && address >= 0x6000) {
		ram[getRamAddress(address)] = value;
	}

	if (isMmioVisible()) {
		writeMmio(address, value, time);
	}

	// write to ramEnable register
	if ((address & 0xF8FF) == 0x48FB) {
		bool newRamEnabled = value & 1;
		if (newRamEnabled != ramEnabled) {
			ramEnabled = newRamEnabled;
			invalidateDeviceRWCache(0x4000, 0x8000);
		}
	}

	// write to bank register
	unsigned int region = 0;
	bool pageSelect = false;
	if (!ramEnabled && (address & 0x1800) == 0x1000) {
		pageSelect = true;
		region = convAddressToRegion(address);
	} else if (ramEnabled && (address & 0xF8FC) == 0x48FC) {
		pageSelect = true;
		region = (address & 3) + 2;
	}
	if (pageSelect) {
		if (value == getBank(region)) return;
		setBank(region, value);
		bankSwitch(region, value);
		// bankSwitch() ends in RomBlocks::setBank(), which fills the CPU's
		// read cache with the ROM block. So this has to come after it: the
		// region may really show RAM, or the SCC window in its upper 2KB,
		// and the CPU would go on reading ROM from the cache.
		invalidateDeviceRWCache(region << 13, 8192);
	}
}

void RomY8960::writeMmio(uint16_t address, byte value, EmuTime time)
{
	if ((address & 0xFFE0) != MmioWindow) return;

	// the tunnels: the low address bit is the address/data half
	if ((address & 0xFFFE) == MmioOpll) {
		if(opll != nullptr) {
			opll->writePort(address & 1, value, time);
		}
	}
	if ((address & 0xFFFE) == MmioOpl2) {
		if(opl2 != nullptr) {
			opl2->writePort(address & 1, value, time);
		}
	}
	if ((address & 0xFFFE) == MmioSsgs) {
		if(ssgs != nullptr) {
			ssgs->writePort(address & 1, value, time);
		}
	}
	// Unlike the other tunnels the OPM's address port is at an odd address,
	// and both ports lie inside the volume range below.
	if ((address == MmioOpmAddress) || (address == MmioOpmData)) {
		if (opm != nullptr) {
			opm->writePort(address == MmioOpmData, value, time);
		}
		return;
	}

	if (address == MmioIoEnabler2) {
		if(opl2 != nullptr) {
			opl2->setIoEnabled((value & Enabler2Opl2) != 0);
		}
		if(ssgs != nullptr) {
			ssgs->setIoEnabled((value & Enabler2Ssgs) != 0);
		}
		if(opm != nullptr) {
			opm->setIoEnabled((value & Enabler2Opm) != 0);
		}
		if(timer != nullptr) {
			timer->setIoEnabled((value & Enabler2Timer) != 0);
		}
	}

	if (address == MmioIoEnabler1) {
		if(opll != nullptr) {
			opll->setIoEnabled((value & Enabler1Opll) != 0);
		}
	}

	if ((address >= MmioVolume) && (address < MmioVolume + Y8960Mixer::ChannelCount)) {
		if (auto* m = getMixer()) {
			m->writeVolume(address - MmioVolume, value);
		}
	}

	if (address == MmioMasterVolume) {
		if (auto* m = getMixer()) {
			m->writeMasterVolume(value);
		}
	}
}

Y8960Mixer* RomY8960::getMixer()
{
	// Looked up on first use: the mixer is created after this device,
	// because it checks that the SCC sound device it mixes exists.
	if ((mixer == nullptr) && !mixerName.empty()) {
		mixer = dynamic_cast<Y8960Mixer*>(getMotherBoard().findDevice(mixerName));
		if (mixer == nullptr) {
			getMotherBoard().getMSXCliComm().printWarning(
				"can not found device '", mixerName, "'.");
			mixerName.clear();
		}
	}
	return mixer;
}

byte* RomY8960::getWriteCacheLine(uint16_t address)
{
	if ((address < 0x4800) || (address >= 0xC000)) {
		return unmappedWrite.data();
	} else if (address < 0x5000) {
		// page selection(0x4800~0x4FFF)
		return nullptr;
	} else if ((address & 0xFF00) == (MmioWindow & CacheLine::HIGH)) {
		// the memory mapped I/O window (0x7F00~0x7FFF)
		return nullptr;
	} else if (isSccAddress(address)) {
		// write to SCC
		return nullptr;
	} else if ((address & 0x1800) == (0x1000 & CacheLine::HIGH)) {
		// page selection
		return nullptr;
	} else if (ramEnabled && isRamRegion(convAddressToRegion(address)) && address >= 0x6000) {
		// write to RAM
		return &ram[getRamAddress(address)];
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomY8960::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("scc",        scc,
				 "bankReg",	   bankReg,
				 "ramEnabled", ramEnabled,
				 "ram",		   ram);
	if (ar.versionBelow(version, 2)) {
		// Version 1 stored this; it is derived from bankReg now.
		bool sccEnabled = false;
		ar.serialize("sccEnabled", sccEnabled);
	}
}
INSTANTIATE_SERIALIZE_METHODS(RomY8960);
REGISTER_MSXDEVICE(RomY8960, "RomY8960");

} // namespace openmsx
