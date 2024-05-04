#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "MSXCliComm.hh"
#include "HardwareConfig.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "stl.hh"
#include "xrange.hh"
#include <bit>
#include <cassert>
#include <iterator>
#include <memory>

namespace openmsx {

AmdFlash::AmdFlash(const Rom& rom, const ValidatedChip& validatedChip,
                   std::span<const bool> writeProtectSectors,
                   const DeviceConfig& config, Load load)
	: motherBoard(config.getMotherBoard())
	, chip(validatedChip.chip)
{
	init(rom.getName() + "_flash", config, load, &rom, writeProtectSectors);
}

AmdFlash::AmdFlash(const std::string& name, const ValidatedChip& validatedChip,
                   std::span<const bool> writeProtectSectors,
                   const DeviceConfig& config)
	: motherBoard(config.getMotherBoard())
	, chip(validatedChip.chip)
{
	init(name, config, Load::NORMAL, nullptr, writeProtectSectors);
}

[[nodiscard]] static bool sramEmpty(const SRAM& ram)
{
	return ranges::all_of(xrange(ram.size()),
	                      [&](auto i) { return ram[i] == 0xFF; });
}

void AmdFlash::init(const std::string& name, const DeviceConfig& config, Load load,
                    const Rom* rom, std::span<const bool> writeProtectSectors)
{
	auto numSectors = chip.geometry.sectorCount;
	assert(writeProtectSectors.size() <= numSectors);

	size_t writableSize = 0;
	size_t readOnlySize = 0;
	writeAddress.resize(numSectors);
	for (size_t sector = 0; const AmdFlash::Region& region : chip.geometry.regions) {
		for (size_t regionSector = 0; regionSector < region.count; regionSector++, sector++) {
			if (sector < writeProtectSectors.size() && writeProtectSectors[sector]) {
				writeAddress[sector] = -1;
				readOnlySize += region.size;
			} else {
				writeAddress[sector] = narrow<ptrdiff_t>(writableSize);
				writableSize += region.size;
			}
		}
	}
	assert((writableSize + readOnlySize) == size());

	bool loaded = false;
	if (writableSize) {
		if (load == Load::NORMAL) {
			ram = std::make_unique<SRAM>(
				name, "flash rom",
				writableSize, config, nullptr, &loaded);
		} else {
			// Hack for 'Matra INK', flash chip is wired-up so that
			// writes are never visible to the MSX (but the flash
			// is not made write-protected). In this case it doesn't
			// make sense to load/save the SRAM file.
			ram = std::make_unique<SRAM>(
				name, "flash rom",
				writableSize, config, SRAM::DontLoadTag{});
		}
	}
	if (readOnlySize) {
		// If some part of the flash is read-only we require a ROM
		// constructor parameter.
		assert(rom);
	}

	const auto* romTag = config.getXML()->findChild("rom");
	bool initialContentSpecified = romTag && romTag->findChild("sha1");

	// check whether the loaded SRAM is empty, whilst initial content was specified
	if (!rom && loaded && initialContentSpecified && sramEmpty(*ram)) {
		config.getCliComm().printInfo(
			"This flash device (", config.getHardwareConfig().getName(),
			") has initial content specified, but this content "
			"was not loaded, because there was already content found "
			"and loaded from persistent storage. However, this "
			"content is blank (it was probably created automatically "
			"when the specified initial content could not be loaded "
			"when this device was used for the first time). If you "
			"still wish to load the specified initial content, "
			"please remove the blank persistent storage file: ",
			ram->getLoadedFilename());
	}

	std::unique_ptr<Rom> rom_;
	if (!rom && !loaded) {
		// If we don't have a ROM constructor parameter and there was
		// no sram content loaded (= previous persistent flash
		// content), then try to load some initial content. This
		// represents the original content of the flash when the device
		// ships. This ROM is optional, if it's not found, then the
		// initial flash content is all 0xFF.
		try {
			rom_ = std::make_unique<Rom>(
				"", "", // dummy name and description
				config);
			rom = rom_.get();
			config.getCliComm().printInfo(
				"Loaded initial content for flash ROM from ",
				rom->getFilename());
		} catch (MSXException& e) {
			// ignore error
			assert(rom == nullptr); // 'rom' remains nullptr
			// only if an actual sha1sum was given, tell the user we failed to use it
			if (initialContentSpecified) {
				config.getCliComm().printWarning(
					"Could not load specified initial content "
					"for flash ROM: ", e.getMessage());
			}
		}
	}

	readAddress.resize(numSectors);
	auto romSize = rom ? rom->size() : 0;
	size_t offset = 0;
	for (size_t sector = 0; const AmdFlash::Region& region : chip.geometry.regions) {
		for (size_t regionSector = 0; regionSector < region.count; regionSector++, sector++) {
			auto sectorSize = region.size;
			if (isSectorWritable(sector)) {
				readAddress[sector] = &(*ram)[writeAddress[sector]];
				if (!loaded) {
					auto* ramPtr = const_cast<uint8_t*>(
						&(*ram)[writeAddress[sector]]);
					if (offset >= romSize) {
						// completely past end of rom
						ranges::fill(std::span{ramPtr, sectorSize}, 0xFF);
					} else if (offset + sectorSize >= romSize) {
						// partial overlap
						auto last = romSize - offset;
						auto missing = sectorSize - last;
						const uint8_t* romPtr = &(*rom)[offset];
						ranges::copy(std::span{romPtr, last}, ramPtr);
						ranges::fill(std::span{&ramPtr[last], missing}, 0xFF);
					} else {
						// completely before end of rom
						const uint8_t* romPtr = &(*rom)[offset];
						ranges::copy(std::span{romPtr, sectorSize}, ramPtr);
					}
				}
			} else {
				assert(rom); // must have rom constructor parameter
				if ((offset + sectorSize) <= romSize) {
					readAddress[sector] = &(*rom)[offset];
				} else {
					readAddress[sector] = nullptr;
				}
			}
			offset += sectorSize;
		}
	}
	assert(offset == size());

	reset();
}

AmdFlash::~AmdFlash() = default;

AmdFlash::GetSectorInfoResult AmdFlash::getSectorInfo(size_t address) const
{
	address &= size() - 1;
	auto it = chip.geometry.regions.begin();
	size_t sector = 0;
	while (address >= it->count * it->size) {
		address -= it->count * it->size;
		sector += it->count;
		++it;
		assert(it != chip.geometry.regions.end());
	}
	return {sector + address / it->size, it->size, address % it->size};
}

void AmdFlash::reset()
{
	cmd.clear();
	setState(State::IDLE);
}

void AmdFlash::setState(State newState)
{
	if (state == newState) return;
	state = newState;
	motherBoard.getCPU().invalidateAllSlotsRWCache(0x0000, 0x10000);
}

uint8_t AmdFlash::peek(size_t address) const
{
	if (state == State::IDLE) {
		auto [sector, sectorSize, offset] = getSectorInfo(address);
		if (const uint8_t* addr = readAddress[sector]) {
			return addr[offset];
		} else {
			return 0xFF;
		}
	} else if (state == State::IDENT) {
		if (chip.geometry.deviceInterface == DeviceInterface::x8x16) {
			if (chip.autoSelect.oddZero && (address & 1)) {
				// some devices return zero for odd bits instead of mirroring
				return 0x00;
			}
			// convert byte address to native address
			address >>= 1;
		}
		return narrow_cast<uint8_t>(peekAutoSelect(address, chip.autoSelect.undefined));
	} else {
		UNREACHABLE;
	}
}

uint16_t AmdFlash::peekAutoSelect(size_t address, uint16_t undefined) const
{
	switch (address & chip.autoSelect.readMask) {
	case 0x0:
		return to_underlying(chip.autoSelect.manufacturer);
	case 0x1:
		return chip.autoSelect.device.size() == 1 ? chip.autoSelect.device[0] | 0x2200 : 0x227E;
	case 0x2:
		if (chip.geometry.deviceInterface == DeviceInterface::x8x16) {
			// convert native address to byte address
			address <<= 1;
		}
		return isSectorWritable(getSectorInfo(address).sector) ? 0 : 1;
	case 0x3:
		// On AM29F040 it indicates "Autoselect Device Unprotect Code".
		// Datasheet does not elaborate. Value is 0x01 according to
		//  https://www.msx.org/forum/semi-msx-talk/emulation/matra-ink-emulated
		//
		// On M29W640G it indicates "Extended Block Verify Code".
		// Bit 7: 0: Extended memory block not factory locked
		//        1: Extended memory block factory locked
		// Other bits are 0x18 on GL, GT and GB models, and 0x01 on GH model.
		// Datasheet does not explain their meaning.
		// Actual value is 0x08 on GB model (verified on hardware).
		//
		// On S29GL064S it indicates "Secure Silicon Region Factory Protect":
		// Bit 4: 0: WP# protects lowest address (bottom boot)
		//        1: WP# protects highest address (top boot)
		// Bit 7: 0: Secure silicon region not factory locked
		//        1: Secure silicon region factory locked
		// Other bits are 0xFF0A. Datasheet does not explain their meaning.
		//
		// On Alliance Memory AS29CF160B it indicates Continuation ID.
		// Datasheet does not elaborate. Value is 0x7F.
		return chip.autoSelect.extraCode;
	case 0xE:
		return chip.autoSelect.device.size() == 2 ? chip.autoSelect.device[0] | 0x2200 : undefined;
	case 0xF:
		return chip.autoSelect.device.size() == 2 ? chip.autoSelect.device[1] | 0x2200 : undefined;
	default:
		// some devices return zero instead of 0xFFFF (typical)
		return undefined;
	}
}

bool AmdFlash::isSectorWritable(size_t sector) const
{
	return vppWpPinLow && (sector == one_of(0u, 1u)) ? false : (writeAddress[sector] != -1) ;
}

uint8_t AmdFlash::read(size_t address) const
{
	// note: after a read we stay in the same mode
	return peek(address);
}

const uint8_t* AmdFlash::getReadCacheLine(size_t address) const
{
	if (state == State::IDLE) {
		auto [sector, sectorSize, offset] = getSectorInfo(address);
		const uint8_t* addr = readAddress[sector];
		return addr ? &addr[offset] : MSXDevice::unmappedRead.data();
	} else {
		return nullptr;
	}
}

void AmdFlash::write(size_t address, uint8_t value)
{
	cmd.push_back({address, value});

	if (state == State::IDLE) {
		if (checkCommandAutoSelect() ||
		    checkCommandEraseSector() ||
		    checkCommandProgram() ||
		    checkCommandDoubleByteProgram() ||
		    checkCommandQuadrupleByteProgram() ||
		    checkCommandEraseChip() ||
		    checkCommandLongReset() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::IDENT) {
		if (checkCommandLongReset() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else {
		UNREACHABLE;
	}
}

// The checkCommandXXX() methods below return
//   true  -> if the command sequence still matches, but is not complete yet
//   false -> if the command was fully matched or does not match with
//            the current command sequence.
// If there was a full match, the command is also executed.
bool AmdFlash::checkCommandReset()
{
	if (cmd[0].value == 0xf0) {
		reset();
	}
	return false;
}

bool AmdFlash::checkCommandLongReset()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0xf0};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 3) return true;
		reset();
	}
	return false;
}

bool AmdFlash::checkCommandEraseSector()
{
	static constexpr std::array<uint8_t, 5> cmdSeq = {0xaa, 0x55, 0x80, 0xaa, 0x55};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 6) return true;
		if (cmd[5].value == 0x30) {
			auto addr = cmd[5].addr;
			auto [sector, sectorSize, offset] = getSectorInfo(addr);
			if (isSectorWritable(sector)) {
				ram->memset(writeAddress[sector],
				            0xff, sectorSize);
			}
		}
	}
	return false;
}

bool AmdFlash::checkCommandEraseChip()
{
	static constexpr std::array<uint8_t, 5> cmdSeq = {0xaa, 0x55, 0x80, 0xaa, 0x55};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 6) return true;
		if (cmd[5].value == 0x10) {
			if (ram) ram->memset(0, 0xff, ram->size());
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgramHelper(size_t numBytes, std::span<const uint8_t> cmdSeq)
{
	if (numBytes <= chip.program.pageSize && partialMatch(cmdSeq)) {
		if (cmd.size() < (cmdSeq.size() + numBytes)) return true;
		for (auto i : xrange(cmdSeq.size(), cmdSeq.size() + numBytes)) {
			auto addr = cmd[i].addr;
			auto [sector, sectorSize, offset] = getSectorInfo(addr);
			if (isSectorWritable(sector)) {
				auto ramAddr = writeAddress[sector] + offset;
				ram->write(ramAddr, (*ram)[ramAddr] & cmd[i].value);
			}
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgram()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0xa0};
	return checkCommandProgramHelper(1, cmdSeq);
}

bool AmdFlash::checkCommandDoubleByteProgram()
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x50};
	return chip.program.fastCommand && checkCommandProgramHelper(2, cmdSeq);
}

bool AmdFlash::checkCommandQuadrupleByteProgram()
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x56};
	return chip.program.fastCommand && checkCommandProgramHelper(4, cmdSeq);
}

bool AmdFlash::checkCommandAutoSelect()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0x90};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 3) return true;
		setState(State::IDENT);
	}
	return false;
}

bool AmdFlash::partialMatch(std::span<const uint8_t> dataSeq) const
{
	static constexpr std::array<unsigned, 5> addrSeq = {0, 1, 0, 0, 1};
	static constexpr std::array<unsigned, 2> cmdAddr = {0x555, 0x2aa};
	(void)addrSeq; (void)cmdAddr; // suppress (invalid) gcc warning

	assert(dataSeq.size() <= 5);
	return ranges::all_of(xrange(std::min(dataSeq.size(), cmd.size())), [&](auto i) {
		// convert byte address to native address
		auto addr = (chip.geometry.deviceInterface == DeviceInterface::x8x16) ? cmd[i].addr >> 1 : cmd[i].addr;
		return ((addr & 0x7FF) == cmdAddr[addrSeq[i]]) &&
		       (cmd[i].value == dataSeq[i]);
	});
}


static constexpr std::initializer_list<enum_string<AmdFlash::State>> stateInfo = {
	{ "IDLE",  AmdFlash::State::IDLE  },
	{ "IDENT", AmdFlash::State::IDENT }
};
SERIALIZE_ENUM(AmdFlash::State, stateInfo);

template<typename Archive>
void AmdFlash::AmdCmd::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("address", addr,
	             "value",   value);
}

// version 1: Initial version.
// version 2: Added vppWpPinLow.
// version 3: Changed cmd to static_vector, added status.
template<typename Archive>
void AmdFlash::serialize(Archive& ar, unsigned version)
{
	ar.serialize("ram", *ram);
	if (ar.versionAtLeast(version, 3)) {
		uint8_t status = 0x80;
		ar.serialize("cmd",    cmd,
		             "status", status);
	} else {
		std::array<AmdCmd, 8> cmdArray;
		unsigned cmdSize = 0;
		ar.serialize("cmd",    cmdArray,
		             "cmdIdx", cmdSize);
		cmd = {from_range, subspan(cmdArray, 0, cmdSize)};
	}
	ar.serialize("state", state);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("vppWpPinLow", vppWpPinLow);
	}
}
INSTANTIATE_SERIALIZE_METHODS(AmdFlash);

} // namespace openmsx
