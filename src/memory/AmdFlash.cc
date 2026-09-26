#include "AmdFlash.hh"

#include "Rom.hh"
#include "SRAM.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "MSXCPU.hh"
#include "MSXCliComm.hh"
#include "MSXDevice.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include "endian.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <zlib.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdio>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>

namespace openmsx {

AmdFlash::AmdFlash(const Rom& rom, const ValidatedChip& validatedChip,
                   std::span<const bool> writeProtectSectors,
                   DeviceConfig& config)
	: AmdFlash(rom.getName() + "_flash", validatedChip, &rom, writeProtectSectors, config, {})
{
}

AmdFlash::AmdFlash(const std::string& name, const ValidatedChip& validatedChip,
                   std::span<const bool> writeProtectSectors,
                   DeviceConfig& config, std::string_view id)
	: AmdFlash(name, validatedChip, nullptr, writeProtectSectors, config, id)
{
}

AmdFlash::AmdFlash(const std::string& name, const ValidatedChip& validatedChip,
                   const Rom* rom, std::span<const bool> writeProtectSectors,
                   DeviceConfig& config, std::string_view id)
	: deviceConfig(config)
	, motherBoard(config.getMotherBoard())
	, chip(validatedChip.chip)
	, syncOperation(motherBoard.getScheduler())
	, syncSuspend(motherBoard.getScheduler())
	, persistentSync(config.getReactor().getRTScheduler())
{
	cmd.reserve(5 + chip.program.bufferPageSize); // longest command is BufferProgram

	assert(writeProtectSectors.size() <= chip.geometry.sectorCount);
	sectors.reserve(chip.geometry.sectorCount);
	for (size_t address = 0; const Region& region : chip.geometry.regions) {
		for (size_t regionSector = 0; regionSector < region.count; ++regionSector, address += region.size) {
			sectors.push_back({
				.index = sectors.size(),
				.address = address,
				.size = region.size,
				.writeProtect = sectors.size() < writeProtectSectors.size() && writeProtectSectors[sectors.size()]
			});
		}
	}

	size_t writableSize = 0;
	size_t readOnlySize = 0;
	for (Sector& sector : sectors) {
		if (sector.writeProtect) {
			sector.writeAddress = -1;
			readOnlySize += sector.size;
		} else {
			sector.writeAddress = narrow<ptrdiff_t>(writableSize);
			writableSize += sector.size;
		}
	}
	assert((writableSize + readOnlySize) == size());

	modifiedSectors.resize(sectors.size(), 0);
	if (writableSize) {
		// Keep SRAM's existing save-state layout, but let Flash own persistence.
		ram = std::make_unique<SRAM>(
			name, "flash rom", writableSize, config, SRAM::DontLoadTag{});
	}
	const bool loaded = ram && loadPersistent();
	if (readOnlySize) {
		// If some part of the flash is read-only we require a ROM
		// constructor parameter.
		assert(rom);
	}

	std::unique_ptr<Rom> rom_;
	if (const bool needsInitialContent = !loaded || std::ranges::any_of(sectors,
		[&](const Sector& s) { return !s.writeProtect && !modifiedSectors[s.index]; });
	    !rom && needsInitialContent) {
		// Without a ROM constructor parameter, load initial content for
		// sectors that have no persistent modifications. This
		// represents the original content of the flash when the device
		// ships. This ROM is optional, if it's not found, then the
		// initial flash content is all 0xFF.
		try {
			rom_ = std::make_unique<Rom>(
				"", "", // dummy name and description
				config, id);
			rom = rom_.get();
			config.getCliComm().printInfo(
				"Loaded initial content for flash ROM from ",
				rom->getFilename());
		} catch (MSXException& e) {
			// ignore error
			assert(rom == nullptr); // 'rom' remains nullptr
			// only if an actual sha1sum was given, tell the user we failed to use it
			const auto* romTag = config.getXML()->findChild("rom");
			const bool initialContentSpecified = romTag && romTag->findChild("sha1");
			if (initialContentSpecified) {
				config.getCliComm().printWarning(
					"Could not load specified initial content "
					"for flash ROM: ", e.getMessage());
			}
		}
	}

	const size_t romSize = rom ? rom->size() : 0;
	size_t offset = 0;
	for (Sector& sector : sectors) {
		if (sector.writeAddress != -1) { // don't use isWritable() here
			sector.readAddress = &(*ram)[sector.writeAddress];
			if (!modifiedSectors[sector.index]) {
				auto ramBlock = ram->getWriteBackdoor().subspan(sector.writeAddress, sector.size);
				if (offset >= romSize) {
					// completely past end of rom
					std::ranges::fill(ramBlock, 0xFF);
				} else if (offset + sector.size >= romSize) {
					// partial overlap
					const size_t last = romSize - offset;
					const uint8_t* romPtr = &(*rom)[offset];
					copy_to_range(std::span{romPtr, last}, ramBlock);
					std::ranges::fill(ramBlock.subspan(last), 0xFF);
				} else {
					// completely before end of rom
					const uint8_t* romPtr = &(*rom)[offset];
					copy_to_range(std::span{romPtr, sector.size}, ramBlock);
				}
			}
		} else {
			assert(rom); // must have rom constructor parameter
			if ((offset + sector.size) <= romSize) {
				sector.readAddress = &(*rom)[offset];
			} else {
				sector.readAddress = nullptr;
			}
		}
		offset += sector.size;
	}
	assert(offset == size());

	reset();
}

AmdFlash::~AmdFlash()
{
	persistentSync.cancelRT();
	try {
		if (persistenceDirty) savePersistent();
	} catch (const std::exception& e) {
		// Destruction must not terminate the emulator on allocation or other
		// unexpected errors. Ordinary file errors are reported by savePersistent.
		fprintf(stderr, "Couldn't save Flash data during destruction: %s\n", e.what());
	}
}

void AmdFlash::discardPendingPersistence()
{
	persistentSync.cancelRT();
	persistenceDirty = false;
}

size_t AmdFlash::refreshUnmodified(const Rom& rom)
{
	// Never alter an in-flight (or suspended) Flash operation or command sequence.
	if (state != State::READ || !cmd.empty() || !program.buffer.empty() ||
	    !erase.buffer.empty() || syncOperation.isPending() || syncSuspend.isPending()) {
		throw MSXException("Cannot refresh Flash during a Flash command; save a state while Flash is idle");
	}
	if (rom.size() > size()) {
		throw MSXException("ROM image exceeds Flash capacity");
	}
	size_t refreshed = 0;
	for (const auto& sector : sectors) {
		if (sector.writeProtect || modifiedSectors[sector.index] || sector.address >= rom.size()) continue;
		auto count = std::min(size_t(sector.size), rom.size() - sector.address);
		auto destination = ram->getWriteBackdoor().subspan(sector.writeAddress, count);
		copy_to_range(std::span{rom}.subspan(sector.address, count), destination);
		refreshed += count;
	}
	// These bytes remain initial ROM content, not persistent guest writes.
	return refreshed;
}

// Based on Laurens Holst's sector persistence design in openMSX PR #1629.
// Metadata and payload share one checksummed file so they cannot get out of sync.
static constexpr std::array<uint8_t, 8> FLASH_FILE_MAGIC = {'O', 'M', 'S', 'X', 'F', 'L', 'S', 1};

bool AmdFlash::loadLegacyPersistent(File& file, const std::string& path)
{
	// Legacy files contain every writable sector, with no history.
	// Preserve all of them rather than infer modifications from a NEW ROM.
	if (file.getSize() != ram->size() || FileOperations::exists(path + ".meta")) {
		throw FileException("Unsupported legacy Flash data size or metadata");
	}
	file.read(ram->getWriteBackdoor());
	for (const Sector& s : sectors) modifiedSectors[s.index] = !s.writeProtect;
	return true;
}

bool AmdFlash::loadPersistent()
{
	const auto& context = deviceConfig.getFileContext();
	const auto filename = deviceConfig.getChildData("sramname");
	std::string path;
	bool sparse = true;
	try {
		path = context.resolveSavePaths(strCat(filename, ".sparse"));
	} catch (FileException&) {
		sparse = false;
		try {
			path = context.resolveSavePaths(filename);
		} catch (FileException&) {
			return false;
		}
	}

	try {
		File file(path);
		if (!sparse) {
			return loadLegacyPersistent(file, path);
		}

		const size_t headerSize = 16 + 5 * sectors.size();
		const size_t fileSize = file.getSize();
		if (fileSize < headerSize + 4 || fileSize > headerSize + ram->size() + 4) {
			throw FileException("Invalid sparse Flash data size");
		}
		std::vector<uint8_t> bytes(fileSize);
		file.read(std::span{bytes});
		if (!std::ranges::equal(std::span{bytes}.first(8), FLASH_FILE_MAGIC) ||
		    Endian::read_UA_L32(bytes.data() + 8) != size() ||
		    Endian::read_UA_L32(bytes.data() + 12) != sectors.size()) {
			throw FileException("Unsupported sparse Flash version or chip geometry");
		}
		if (const auto crc = crc32(0, bytes.data(), narrow<uInt>(bytes.size() - 4));
		    crc != Endian::read_UA_L32(bytes.data() + bytes.size() - 4)) {
			throw FileException("Sparse Flash checksum mismatch");
		}
		size_t expectedSize = headerSize + 4;
		for (const Sector& s : sectors) {
			const auto* entry = bytes.data() + 16 + 5 * s.index;
			if (Endian::read_UA_L32(entry) != s.size || entry[4] > 1 ||
			    (s.writeProtect && entry[4])) {
				throw FileException("Invalid sparse Flash sector metadata");
			}
			if (entry[4]) expectedSize += s.size;
		}
		if (expectedSize != bytes.size()) throw FileException("Sparse Flash payload size mismatch");

		// Apply only after validating the entire file. Never partially load a save.
		auto data = ram->getWriteBackdoor();
		size_t offset = headerSize;
		for (const Sector& s : sectors) {
			modifiedSectors[s.index] = bytes[16 + 5 * s.index + 4];
			if (modifiedSectors[s.index]) {
				copy_to_range(std::span{bytes}.subspan(offset, s.size), data.subspan(s.writeAddress, s.size));
				offset += s.size;
			}
		}
		return true;
	} catch (FileException& e) {
		// Refuse to mount bad data instead of silently starting blank and later
		// overwriting the only copy of the player's progress.
		throw MSXException("Couldn't load Flash data ", path, " (", e.getMessage(), ").");
	}
}

void AmdFlash::schedulePersistentSave()
{
	persistenceDirty = true;
	if (!persistentSync.isPendingRT()) persistentSync.scheduleRT(5000000);
}

void AmdFlash::markModified(const Sector& sector)
{
	modifiedSectors[sector.index] = 1;
	schedulePersistentSave();
}

void AmdFlash::savePersistent()
{
	const auto filename = strCat(deviceConfig.getChildData("sramname"), ".sparse");
	std::string temporary;
	try {
		const size_t headerSize = 16 + 5 * sectors.size();
		std::vector<uint8_t> bytes(headerSize);
		copy_to_range(FLASH_FILE_MAGIC, std::span{bytes}.first(8));
		Endian::write_UA_L32(bytes.data() + 8, narrow<uint32_t>(size_t(size())));
		Endian::write_UA_L32(bytes.data() + 12, narrow<uint32_t>(sectors.size()));
		for (const Sector& s : sectors) {
			Endian::write_UA_L32(bytes.data() + 16 + 5 * s.index, narrow<uint32_t>(size_t(s.size)));
			bytes[16 + 5 * s.index + 4] = modifiedSectors[s.index];
		}
		for (const Sector& s : sectors) {
			if (!modifiedSectors[s.index]) continue;
			const auto data = std::span{&(*ram)[s.writeAddress], size_t(s.size)};
			bytes.insert(bytes.end(), data.begin(), data.end());
		}
		const auto crc = crc32(0, bytes.data(), narrow<uInt>(bytes.size()));
		bytes.resize(bytes.size() + 4);
		Endian::write_UA_L32(bytes.data() + bytes.size() - 4, narrow<uint32_t>(crc));

		const auto path = deviceConfig.getFileContext().resolveCreate(filename);
		auto file = FileOperations::openUniqueFile(std::string(FileOperations::getDirName(path)), temporary);
		if (!file || fwrite(bytes.data(), 1, bytes.size(), file.get()) != bytes.size() ||
		    fflush(file.get()) != 0) {
			throw FileException("Couldn't write temporary Flash data");
		}
		file.reset();
		FileOperations::replaceFile(temporary, path);
		persistenceDirty = false;
	} catch (FileException& e) {
		if (!temporary.empty()) FileOperations::unlink(temporary);
		deviceConfig.getCliComm().printWarning("Couldn't save Flash data ", filename, " (", e.getMessage(), ").");
	}
}

size_t AmdFlash::getSectorIndex(size_t address) const
{
	size_t sectorIndex = 0;
	for (const Region& region : chip.geometry.regions) {
		if (address < region.count * region.size) {
			return sectorIndex + address / region.size;
		} else {
			address -= region.count * region.size;
			sectorIndex += region.count;
		}
	}
	UNREACHABLE;
}

bool AmdFlash::isWritable(const Sector& sector) const
{
	if (vppWpPinLow) {
		const auto range = chip.geometry.writeProtectPinRange;
		if ((range > 0 && sector <= sectors[range - 1uz]) ||
		    (range < 0 && sector >= sectors[range + chip.geometry.sectorCount])) {
			return false;
		}
	}
	return !sector.writeProtect;
}

bool AmdFlash::getReadyPin() const
{
	return !chip.misc.readyPin || (statusRegister.ready && !status.error && !status.abort);
}

void AmdFlash::reset()
{
	cmd.clear();
	status = {};
	statusRegister = {};
	setState(State::READ);
	syncOperation.removeSyncPoint();
	syncSuspend.removeSyncPoint();
	erase.reset();
	program.reset();
}

void AmdFlash::softReset()
{
	clearStatus();
	setState(State::READ);
}

void AmdFlash::clearStatus()
{
	status.dataPolling = false;
	status.busyToggle = false;
	status.error = false;
	status.eraseTimer = false;
	status.eraseToggle = false;
	status.abort = false;
	statusRegister.eraseStatus = false;
	statusRegister.programStatus = false;
	statusRegister.bufferAbort = false;
	statusRegister.sectorLock = false;
	statusRegister.continuity = false;
}

void AmdFlash::setState(State newState)
{
	if (state == newState) return;
	state = newState;
	motherBoard.getCPU().invalidateAllSlotsRWCache(0x0000, 0x10000);
}

uint8_t AmdFlash::read(size_t address, EmuTime time)
{
	address %= size();
	const uint8_t value = peek(address, time);
	if (state == State::READ) {
		if (statusRegister.eraseSuspend && erase.isPending(getSector(address))) {
			status.eraseToggle = !status.eraseToggle;
		}
	} else if (state == State::STATUS) {
		setState(State::READ);
	} else if (state == one_of(State::PROGRAM, State::ERROR)) {
		status.busyToggle = !status.busyToggle;
	} else if (state == State::ERASE_SECTOR) {
		status.busyToggle = !status.busyToggle;
		if (erase.isPending(getSector(address))) {
			status.eraseToggle = !status.eraseToggle;
		}
	} else if (state == State::ERASE_CHIP) {
		status.busyToggle = !status.busyToggle;
		status.eraseToggle = !status.eraseToggle;
	}
	return value;
}

uint8_t AmdFlash::peek(size_t address, EmuTime /*time*/) const
{
	address %= size();
	if (state == State::READ) {
		const Sector& sector = getSector(address);
		if (erase.isPending(sector)) {
			return status.raw;
		} else {
			return sector.readAddress ? sector.readAddress[address - sector.address] : 0xFF;
		}
	} else if (state == State::AUTOSELECT) {
		if (chip.geometry.deviceInterface == DeviceInterface::x8x16) {
			if (chip.autoSelect.oddZero && (address & 1)) {
				// some devices return zero for odd bits instead of mirroring
				return 0x00;
			}
			// convert byte address to native address
			address >>= 1;
		}
		return narrow_cast<uint8_t>(peekAutoSelect(address, chip.autoSelect.undefined));
	} else if (state == State::CFI) {
		if (chip.geometry.deviceInterface == DeviceInterface::x8x16) {
			// convert byte address to native address
			const uint16_t result = peekCFI(address >> 1);
			return narrow_cast<uint8_t>(address & 1 ? result >> 8 : result);
		} else {
			return narrow_cast<uint8_t>(peekCFI(address));
		}
	} else if (state == State::STATUS) {
		return statusRegister.raw;
	} else if (state == one_of(State::ERASE_SECTOR, State::ERASE_CHIP, State::PROGRAM, State::ERROR)) {
		return status.raw;
	} else {
		UNREACHABLE;
	}
}

uint16_t AmdFlash::peekAutoSelect(size_t address, uint16_t undefined) const
{
	switch (address & chip.autoSelect.readMask) {
	case 0x0:
		return std::to_underlying(chip.autoSelect.manufacturer);
	case 0x1:
		return chip.autoSelect.device.size() == 1 ? chip.autoSelect.device[0] | 0x2200 : 0x227E;
	case 0x2:
		if (address & 0x40) {
			return undefined;
		}
		if (chip.geometry.deviceInterface == DeviceInterface::x8x16) {
			// convert native address to byte address
			address <<= 1;
		}
		return isWritable(getSector(address)) ? 0 : 1;
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

uint16_t AmdFlash::peekCFI(size_t address) const
{
	const size_t maskedAddress = address & chip.cfi.readMask;

	if (maskedAddress < 0x10) {
		if (chip.cfi.withManufacturerDevice) {
			// M29W640GB exposes manufacturer & device ID below 0x10 (as 16-bit values)
			switch (maskedAddress) {
			case 0x0:
				return std::to_underlying(chip.autoSelect.manufacturer);
			case 0x1:
				return chip.autoSelect.device.size() == 1 ? chip.autoSelect.device[0] | 0x2200 : 0x227E;
			case 0x2:
				return chip.autoSelect.device.size() == 2 ? chip.autoSelect.device[0] | 0x2200 : 0xFFFF;
			case 0x3:
				return chip.autoSelect.device.size() == 2 ? chip.autoSelect.device[1] | 0x2200 : 0xFFFF;
			default:
				return 0xFFFF;
			}
		} else if (chip.cfi.withAutoSelect) {
			// S29GL064S exposes auto select data below 0x10 (as 16-bit values)
			return peekAutoSelect(address | 0x40);
		}
	}

	switch (maskedAddress) {
	case 0x10:
		return 'Q';
	case 0x11:
		return 'R';
	case 0x12:
		return 'Y';
	case 0x13:
		return 0x02;  // AMD compatible
	case 0x14:
		return 0x00;
	case 0x15:
		return 0x40;
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1A:
		return 0x00;
	case 0x1B:
		return chip.cfi.systemInterface.supply.minVcc;
	case 0x1C:
		return chip.cfi.systemInterface.supply.maxVcc;
	case 0x1D:
		return chip.cfi.systemInterface.supply.minVpp;
	case 0x1E:
		return chip.cfi.systemInterface.supply.maxVpp;
	case 0x1F:
		return chip.cfi.systemInterface.typTimeout.singleProgram.exponent;
	case 0x20:
		return chip.cfi.systemInterface.typTimeout.multiProgram.exponent;
	case 0x21:
		return chip.cfi.systemInterface.typTimeout.sectorErase.exponent;
	case 0x22:
		return chip.cfi.systemInterface.typTimeout.chipErase.exponent;
	case 0x23:
		return chip.cfi.systemInterface.maxTimeoutMult.singleProgram.exponent;
	case 0x24:
		return chip.cfi.systemInterface.maxTimeoutMult.multiProgram.exponent;
	case 0x25:
		return chip.cfi.systemInterface.maxTimeoutMult.sectorErase.exponent;
	case 0x26:
		return chip.cfi.systemInterface.maxTimeoutMult.chipErase.exponent;
	case 0x27:
		return chip.geometry.size.exponent;
	case 0x28:
		return narrow<uint8_t>(std::to_underlying(chip.geometry.deviceInterface) & 0xFF);
	case 0x29:
		return narrow<uint8_t>(std::to_underlying(chip.geometry.deviceInterface) >> 8);
	case 0x2A:
		return std::max(chip.program.fastPageSize.exponent, chip.program.bufferPageSize.exponent);
	case 0x2B:
		return 0x00;
	case 0x2C:
		return narrow<uint8_t>(chip.geometry.regions.size());
	case 0x2D:
	case 0x31:
	case 0x35:
	case 0x39:
		if (const size_t index = (maskedAddress - 0x2D) >> 2; index < chip.geometry.regions.size()) {
			return narrow<uint8_t>((chip.geometry.regions[index].count - 1) & 0xFF);
		}
		return 0x00;
	case 0x2E:
	case 0x32:
	case 0x36:
	case 0x3A:
		if (const size_t index = (maskedAddress - 0x2E) >> 2; index < chip.geometry.regions.size()) {
			return narrow<uint8_t>((chip.geometry.regions[index].count - 1) >> 8);
		}
		return 0x00;
	case 0x2F:
	case 0x33:
	case 0x37:
	case 0x3B:
		if (const size_t index = (maskedAddress - 0x2F) >> 2; index < chip.geometry.regions.size()) {
			return narrow<uint8_t>((chip.geometry.regions[index].size >> 8) & 0xFF);
		}
		return 0x00;
	case 0x30:
	case 0x34:
	case 0x38:
	case 0x3C:
		if (const size_t index = (maskedAddress - 0x30) >> 2; index < chip.geometry.regions.size()) {
			return narrow<uint8_t>((chip.geometry.regions[index].size >> 8) >> 8);
		}
		return 0x00;
	case 0x40:
		return 'P';
	case 0x41:
		return 'R';
	case 0x42:
		return 'I';
	case 0x43:
		return '0' + chip.cfi.primaryAlgorithm.version.major;
	case 0x44:
		return '0' + chip.cfi.primaryAlgorithm.version.minor;
	case 0x45:
		return narrow<uint8_t>(chip.cfi.primaryAlgorithm.addressSensitiveUnlock | (chip.cfi.primaryAlgorithm.siliconRevision << 2));
	case 0x46:
		return chip.cfi.primaryAlgorithm.eraseSuspend;
	case 0x47:
		return chip.cfi.primaryAlgorithm.sectorProtect;
	case 0x48:
		return chip.cfi.primaryAlgorithm.sectorTemporaryUnprotect;
	case 0x49:
		return chip.cfi.primaryAlgorithm.sectorProtectScheme;
	case 0x4A:
		return chip.cfi.primaryAlgorithm.simultaneousOperation;
	case 0x4B:
		return chip.cfi.primaryAlgorithm.burstMode;
	case 0x4C:
		return chip.cfi.primaryAlgorithm.pageMode;
	case 0x4D:
		return chip.cfi.primaryAlgorithm.version.minor >= 1 ? chip.cfi.primaryAlgorithm.supply.minAcc : 0xFFFF;
	case 0x4E:
		return chip.cfi.primaryAlgorithm.version.minor >= 1 ? chip.cfi.primaryAlgorithm.supply.maxAcc : 0xFFFF;
	case 0x4F:
		return chip.cfi.primaryAlgorithm.version.minor >= 1 ? chip.cfi.primaryAlgorithm.bootBlockFlag : 0xFFFF;
	case 0x50:
		return chip.cfi.primaryAlgorithm.version.minor >= 2 ? chip.cfi.primaryAlgorithm.programSuspend : 0xFFFF;
	default:
		// TODO
		// 0x61-0x64 Unique 64-bit device number / security code for M29W800DB and M29W640GB.
		// M29W640GB also has unknown data in 0x65-0x6A, as well as some in the 0x80-0xFF range.
		return 0xFFFF;
	}
}

const uint8_t* AmdFlash::getReadCacheLine(size_t address) const
{
	address %= size();
	if (state == State::READ) {
		const Sector& sector = getSector(address);
		if (erase.isPending(sector)) {
			return nullptr;
		} else {
			const uint8_t* addr = sector.readAddress;
			return addr ? &addr[address - sector.address] : MSXDevice::unmappedRead.data();
		}
	} else {
		return nullptr;
	}
}

void AmdFlash::write(size_t address, uint8_t value, EmuTime time)
{
	address %= size();
	cmd.push_back({.addr = address, .value = value});

	if (state == State::READ) {
		if (checkCommandAutoSelect() ||
		    checkCommandEraseSector(time) ||
		    checkCommandEraseChip(time) ||
		    checkCommandProgram(time) ||
		    checkCommandDoubleByteProgram(time) ||
		    checkCommandQuadrupleByteProgram(time) ||
		    checkCommandBufferProgram(time) ||
		    checkCommandResume(time) ||
		    checkCommandCFIQuery() ||
		    checkCommandContinuityCheck() ||
		    checkCommandStatusRead() ||
		    checkCommandStatusClear() ||
		    checkCommandLongReset() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::AUTOSELECT) {
		if (checkCommandCFIQuery() ||
		    checkCommandLongReset() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::CFI) {
		if (checkCommandLongReset() ||
		    checkCommandCFIExit() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::STATUS) {
		// TODO confirm. S29GL064S datasheet: "it is not recommended".
		if (checkCommandLongReset() ||
		    checkCommandReset()) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::ERROR) {
		const bool shortReset = !status.abort || chip.program.shortAbortReset;
		if (checkCommandLongReset() ||
		    (shortReset && checkCommandReset())) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::ERASE_SECTOR) {
		if (checkCommandEraseAdditionalSector(time) ||
		    checkCommandSuspend(time)) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::ERASE_CHIP) {
		if (false) {
			// do nothing, we're still matching a command, but it is not complete yet
		} else {
			cmd.clear();
		}
	} else if (state == State::PROGRAM) {
		if (checkCommandSuspend(time)) {
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
		softReset();
	}
	return false;
}

bool AmdFlash::checkCommandLongReset()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0xf0};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 3) return true;
		softReset();
	}
	return false;
}

bool AmdFlash::checkCommandAutoSelect()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0x90};
	if (partialMatch(cmdSeq)) {
		if (cmd.size() < 3) return true;
		setState(State::AUTOSELECT);
	}
	return false;
}

bool AmdFlash::checkCommandCFIQuery()
{
	// convert byte address to native address
	const size_t addr = (chip.geometry.deviceInterface == DeviceInterface::x8x16) ? cmd[0].addr >> 1 : cmd[0].addr;
	if (chip.cfi.command && (addr & chip.cfi.commandMask) == 0x55 && cmd[0].value == 0x98) {
		setState(State::CFI);
	}
	return false;
}

bool AmdFlash::checkCommandCFIExit()
{
	if (chip.cfi.exitCommand && cmd[0].value == 0xff) {
		softReset();
	}
	return false;
}

bool AmdFlash::checkCommandStatusRead()
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x70};
	if (chip.misc.statusCommand && partialMatch(cmdSeq)) {
		setState(State::STATUS);
	}
	return false;
}

bool AmdFlash::checkCommandStatusClear()
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x71};
	if (chip.misc.statusCommand && partialMatch(cmdSeq)) {
		softReset();
	}
	return false;
}

bool AmdFlash::checkCommandContinuityCheck()
{
	if (chip.misc.continuityCommand && cmd[0] == AddressValue{.addr = 0x5554AB, .value = 0xFF}) {
		if (cmd.size() < 2) return true;
		if (cmd.size() == 2 && cmd[1] == AddressValue{.addr = 0x2AAB54, .value = 0x00}) {
			statusRegister.continuity = true;
		}
	}
	return false;
}

bool AmdFlash::checkCommandEraseSector(EmuTime time)
{
	static constexpr std::array<uint8_t, 5> cmdSeq = {0xaa, 0x55, 0x80, 0xaa, 0x55};
	if (!statusRegister.eraseSuspend && !statusRegister.programSuspend && partialMatch(cmdSeq)) {
		if (cmd.size() < 6) return true;
		if (cmd[5].value == 0x30) {
			clearStatus();

			if (const Sector& sector = getSector(cmd[5].addr);
			    isWritable(sector)) {
				erase.markPending(sector);
			} else {
				statusRegister.sectorLock = true;
			}

			statusRegister.ready = false;
			status.dataPolling = false;
			setState(State::ERASE_SECTOR);
			scheduleEraseOperation(time);
		}
	}
	return false;
}

bool AmdFlash::checkCommandEraseAdditionalSector(EmuTime time)
{
	assert(state == State::ERASE_SECTOR);
	if (!status.eraseTimer && cmd[0].value == 0x30) {
		if (const Sector& sector = getSector(cmd[0].addr);
			isWritable(sector)) {
			erase.markPending(sector);
		} else {
			statusRegister.sectorLock = true;
		}

		scheduleEraseOperation(time);
	}
	return false;
}

bool AmdFlash::checkCommandEraseChip(EmuTime time)
{
	static constexpr std::array<uint8_t, 5> cmdSeq = {0xaa, 0x55, 0x80, 0xaa, 0x55};
	if (!statusRegister.eraseSuspend && !statusRegister.programSuspend && partialMatch(cmdSeq)) {
		if (cmd.size() < 6) return true;
		if (cmd[5].value == 0x10) {
			clearStatus();

			for (const Sector& sector : sectors) {
				if (isWritable(sector)) {
					erase.markPending(sector);
				} else {
					statusRegister.sectorLock = true;
				}
			}

			statusRegister.ready = false;
			status.dataPolling = false;
			setState(State::ERASE_CHIP);
			scheduleEraseOperation(time);
		}
	}
	return false;
}

void AmdFlash::scheduleEraseOperation(EmuTime time)
{
	if (state == State::ERASE_SECTOR && !status.eraseTimer) {
		syncOperation.removeSyncPoint();
		syncOperation.setSyncPoint(time + chip.erase.timerDuration);
	} else if (std::ranges::any_of(erase.buffer, std::identity{})) {
		assert(!syncOperation.isPending());
		syncOperation.setSyncPoint(time + chip.erase.duration);
	} else {
		assert(!syncOperation.isPending());
		syncOperation.setSyncPoint(time + chip.erase.minimumDuration);
	}
}

void AmdFlash::execEraseOperation(EmuTime time)
{
	assert(state == one_of(State::ERASE_SECTOR, State::ERASE_CHIP));
	if (state == State::ERASE_SECTOR && !status.eraseTimer) {
		status.eraseTimer = true;
		scheduleEraseOperation(time);
	} else {
		while (erase.index < sectors.size()) {
			const Sector& sector = sectors[erase.index];
			const bool pending = erase.isPending(sector);
			++erase.index;
			if (pending) {
				assert(!sector.writeProtect);
				ram->memset(sector.writeAddress, 0xff, sector.size);
				markModified(sector);
				break;
			}
		}

		if (erase.index >= sectors.size()) {
			erase.reset();
			statusRegister.ready = true;
			status.dataPolling = true;
			status.eraseTimer = false;
			setState(State::READ);
		} else {
			syncOperation.setSyncPoint(time + chip.erase.duration);
		}
	}
}

bool AmdFlash::EraseOperation::isPending(const Sector& sector) const
{
	return index <= sector.index && sector.index < buffer.size() && buffer[sector.index];
}

void AmdFlash::EraseOperation::markPending(const Sector& sector)
{
	if (sector.index >= buffer.size()) {
		buffer.resize(sector.index + 1, false);
	}
	buffer[sector.index] = true;
}

void AmdFlash::EraseOperation::reset()
{
	index = 0;
	buffer.clear();
}

bool AmdFlash::checkCommandSuspend(EmuTime time)
{
	if (state == State::ERASE_SECTOR) {
		if (chip.erase.suspend) {
			if (cmd[0].value == 0xB0) {
				if (!status.eraseTimer) {
					syncOperation.removeSyncPoint();
					execSuspend(time);
				} else {
					if (!syncSuspend.isPending()) {
						syncOperation.removeSyncPoint();
						syncSuspend.setSyncPoint(time + chip.erase.suspendDuration);
					}
				}
			}
		}
	} else if (state == State::PROGRAM) {
		if (chip.program.suspend) {
			if (cmd[0].value == 0xB0 || (chip.program.enhancedSuspend && cmd[0].value == 0x51)) {
				if (!syncSuspend.isPending()) {
					syncOperation.removeSyncPoint();
					syncSuspend.setSyncPoint(time + chip.program.suspendDuration);
				}
			}
		}
	} else {
		UNREACHABLE;
	}
	return false;
}

void AmdFlash::execSuspend(EmuTime /*time*/)
{
	if (state == State::ERASE_SECTOR) {
		statusRegister.ready = true;
		statusRegister.eraseSuspend = true;
		status.dataPolling = true;
		status.eraseTimer = true;
		setState(State::READ);
	} else if (state == State::PROGRAM) {
		statusRegister.ready = true;
		statusRegister.programSuspend = true;
		setState(State::READ);
	} else {
		UNREACHABLE;
	}
}

bool AmdFlash::checkCommandResume(EmuTime time)
{
	if (statusRegister.programSuspend) {
		if (cmd[0].value == 0x30 || (chip.program.enhancedSuspend && cmd[0].value == 0x50)) {
			statusRegister.ready = false;
			statusRegister.programSuspend = false;
			setState(State::PROGRAM);
			scheduleProgramOperation(time);
		}
	} else if (statusRegister.eraseSuspend) {
		if (cmd[0].value == 0x30) {
			statusRegister.ready = false;
			statusRegister.eraseSuspend = false;
			status.dataPolling = false;
			setState(State::ERASE_SECTOR);
			scheduleEraseOperation(time);
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgram(EmuTime time)
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0xa0};
	return checkCommandProgramHelper(1, cmdSeq, time);
}

bool AmdFlash::checkCommandDoubleByteProgram(EmuTime time)
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x50};
	return checkCommandProgramHelper(2, cmdSeq, time);
}

bool AmdFlash::checkCommandQuadrupleByteProgram(EmuTime time)
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x56};
	return checkCommandProgramHelper(4, cmdSeq, time);
}

bool AmdFlash::checkCommandProgramHelper(size_t numBytes, std::span<const uint8_t> cmdSeq, EmuTime time)
{
	if (numBytes <= chip.program.fastPageSize && partialMatch(cmdSeq)) {
		if (cmd.size() == cmdSeq.size()) clearStatus();
		if (cmd.size() <= cmdSeq.size()) return true;
		status.dataPolling = ~cmd.back().value & 0x80;
		if (cmd.size() < (cmdSeq.size() + numBytes)) return true;
		const Sector& sector = getSector(cmd.back().addr);
		if (isWritable(sector) && !erase.isPending(sector)) {
			const size_t pageMask = chip.program.fastPageSize - 1;
			program.address = cmd.back().addr & ~pageMask;
			program.buffer.assign(chip.program.fastPageSize, 0xFFFF);

			// For fast program commands, if you write 0x7F and then 0xF7 to the same empty
			// flash memory location within the same command, the final value is 0x77.
			// Tested on: M29W640GB.
			for (size_t i : xrange(cmdSeq.size(), cmdSeq.size() + numBytes)) {
				program.buffer[cmd[i].addr & pageMask] &= cmd[i].value;
			}

			statusRegister.ready = false;
			setState(State::PROGRAM);
			scheduleProgramOperation(time);
		} else {
			// immediate completion
			statusRegister.sectorLock = true;
			status.dataPolling = !status.dataPolling || statusRegister.eraseSuspend;
		}
	}
	return false;
}

bool AmdFlash::checkCommandBufferProgram(EmuTime time)
{
	static constexpr std::array<uint8_t, 2> cmdSeq = {0xaa, 0x55};
	if (chip.program.bufferPageSize > 1 && partialMatch(cmdSeq) && (cmd.size() <= 2 || cmd[2].value == 0x25)) {
		if (cmd.size() <= 3) {
			if (cmd.size() == 3) clearStatus();
			return true;
		} else if (cmd.size() <= 4) {
			if (cmd[3].value < chip.program.bufferPageSize && getSector(cmd[3].addr) == getSector(cmd[2].addr)) {
				return true;
			}
		} else if (cmd.size() <= 5) {
			if (getSector(cmd[4].addr) == getSector(cmd[2].addr)) {
				status.dataPolling = ~cmd.back().value & 0x80;
				return true;
			}
		} else if (cmd.size() <= size_t(5 + cmd[3].value)) {
			const size_t pageMask = chip.program.bufferPageSize - 1;
			if ((cmd.back().addr & ~pageMask) == (cmd[4].addr & ~pageMask)) {
				status.dataPolling = ~cmd.back().value & 0x80;
				return true;
			}
		} else {
			const Sector& sector = getSector(cmd[2].addr);
			if (cmd.back().value == 0x29 && getSector(cmd.back().addr) == sector) {
				if (isWritable(sector) && !erase.isPending(sector)) {
					const size_t pageMask = chip.program.bufferPageSize - 1;
					program.address = cmd[4].addr & ~pageMask;
					program.buffer.assign(chip.program.bufferPageSize, 0xFFFF);

					// For buffer program commands, if you write 0x7F and then 0xF7 to the same empty
					// flash memory location within the same command, the final value is 0xF7.
					// Tested on: M29W640GB, S29GL064S.
					for (size_t i : xrange<size_t>(4, cmd.size() - 1)) {
						program.buffer[cmd[i].addr & pageMask] = cmd[i].value;
					}

					statusRegister.ready = false;
					setState(State::PROGRAM);
					scheduleProgramOperation(time);
				} else {
					// immediate completion
					statusRegister.sectorLock = true;
					status.dataPolling = !status.dataPolling || statusRegister.eraseSuspend;
				}
				return false;
			}
		}
		status.abort = true;
		statusRegister.programStatus = true;
		statusRegister.bufferAbort = true;
		setState(State::ERROR);
	}
	return false;
}

void AmdFlash::scheduleProgramOperation(EmuTime time)
{
	assert(!syncOperation.isPending());
	syncOperation.setSyncPoint(time + chip.program.duration);
}

void AmdFlash::execProgramOperation(EmuTime time)
{
	assert(state == State::PROGRAM);
	const Sector& sector = getSector(program.address);
	assert(!sector.writeProtect);
	size_t ramAddr = program.address - sector.address + sector.writeAddress + program.index;
	while (program.index < program.buffer.size()) {
		const uint16_t value = program.buffer[program.index];
		++program.index;
		if (value < 0x100) {
			const uint8_t ramValue = (*ram)[ramAddr] & value;
			ram->write(ramAddr, ramValue);
			markModified(sector);
			if (chip.program.bitRaiseError && ramValue != value) {
				program.reset();
				status.error = true;
				setState(State::ERROR);
				return;
			}
			break;
		}
		++ramAddr;
	}

	if (program.index >= program.buffer.size()) {
		program.reset();
		statusRegister.ready = true;
		status.dataPolling = !status.dataPolling || statusRegister.eraseSuspend;
		setState(State::READ);
	} else {
		syncOperation.setSyncPoint(time + chip.program.additionalDuration);
	}
}

void AmdFlash::ProgramOperation::reset()
{
	address = 0;
	index = 0;
	buffer.clear();
}

bool AmdFlash::partialMatch(std::span<const uint8_t> dataSeq) const
{
	static constexpr std::array<unsigned, 5> addrSeq = {0, 1, 0, 0, 1};
	static constexpr std::array<unsigned, 2> cmdAddr = {0x555, 0x2aa};
	(void)addrSeq; (void)cmdAddr; // suppress (invalid) gcc warning

	assert(dataSeq.size() <= 5);
	return std::ranges::all_of(xrange(std::min(dataSeq.size(), cmd.size())), [&](const size_t i) {
		// convert byte address to native address
		const size_t addr = (chip.geometry.deviceInterface == DeviceInterface::x8x16) ? cmd[i].addr >> 1 : cmd[i].addr;
		return ((addr & 0x7FF) == cmdAddr[addrSeq[i]]) &&
		       (cmd[i].value == dataSeq[i]);
	});
}

void AmdFlash::execOperation(EmuTime time)
{
	if (state == one_of(State::ERASE_SECTOR, State::ERASE_CHIP)) {
		execEraseOperation(time);
	} else if (state == State::PROGRAM) {
		execProgramOperation(time);
	} else {
		UNREACHABLE;
	}
}

static constexpr auto stateInfo = std::to_array<enum_string<AmdFlash::State>>({
	{ "IDLE",         AmdFlash::State::READ },       // back compat with v3
	{ "IDENT",        AmdFlash::State::AUTOSELECT }, // back compat with v3
	{ "PRGERR",       AmdFlash::State::ERROR },      // back compat with v3
	{ "READ",         AmdFlash::State::READ  },
	{ "AUTOSELECT",   AmdFlash::State::AUTOSELECT },
	{ "CFI",          AmdFlash::State::CFI },
	{ "STATUS",       AmdFlash::State::STATUS },
	{ "ERROR",        AmdFlash::State::ERROR },
	{ "ERASE_SECTOR", AmdFlash::State::ERASE_SECTOR },
	{ "ERASE_CHIP",   AmdFlash::State::ERASE_CHIP },
	{ "PROGRAM",      AmdFlash::State::PROGRAM },
});
SERIALIZE_ENUM(AmdFlash::State, stateInfo);

template<typename Archive>
void AmdFlash::AddressValue::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("address", addr,
	             "value",   value);
}

template<typename Archive>
void AmdFlash::EraseOperation::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("index", index,
	             "buffer", buffer);
}

template<typename Archive>
void AmdFlash::ProgramOperation::serialize(Archive& ar, unsigned /*version*/)
{
		ar.serialize("address", address,
		             "index", index,
		             "buffer", buffer);
}

void AmdFlash::validateModifiedSectors() const
{
	if (modifiedSectors.size() != sectors.size()) {
		throw MSXException("Invalid Flash save-state sector count");
	}
	for (const Sector& s : sectors) {
		if (modifiedSectors[s.index] > 1 || (s.writeProtect && modifiedSectors[s.index])) {
			throw MSXException("Invalid Flash save-state sector flags");
		}
	}
}

// version 1: Initial version.
// version 2: Added vppWpPinLow.
// version 3: Changed cmd to static_vector, added status.
// version 4: Added status register, sectors and operation / suspend schedulables.
// version 5: Track sectors modified by the emulated Flash, including erased sectors.
template<typename Archive>
void AmdFlash::serialize(Archive& ar, unsigned version)
{
	ar.serialize("ram", *ram);
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("modifiedSectors", modifiedSectors);
		if constexpr (Archive::IS_LOADER) {
			validateModifiedSectors();
		}
	} else if constexpr (Archive::IS_LOADER) {
		// Old snapshots have no modification history. Never guess away save data.
		for (const Sector& s : sectors) modifiedSectors[s.index] = !s.writeProtect;
	}
	if constexpr (Archive::IS_LOADER) {
		// Includes an empty mask: restoring a pre-write state must supersede the
		// previously persisted sectors too. Reverse snapshots retain this mask.
		schedulePersistentSave();
	}
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("cmd",    cmd,
		             "status", status.raw);
	} else {
		std::array<AddressValue, 8> cmdArray;
		unsigned cmdSize = 0;
		ar.serialize("cmd",    cmdArray,
		             "cmdIdx", cmdSize);
		cmd.assign(cmdArray.begin(), cmdArray.begin() + cmdSize);
	}
	ar.serialize("state", state);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("vppWpPinLow", vppWpPinLow);
	}
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("statusRegister", statusRegister.raw,
		             "erase", erase,
					 "program", program,
		             "syncOperation", syncOperation,
		             "syncSuspend", syncSuspend);
	}
}
INSTANTIATE_SERIALIZE_METHODS(AmdFlash);

} // namespace openmsx
