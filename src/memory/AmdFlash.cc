#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "CliComm.hh"
#include "HardwareConfig.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cstring>
#include <cassert>
#include <iterator>
#include <memory>

using std::string;

namespace openmsx {

AmdFlash::AmdFlash(const Rom& rom, span<const SectorInfo> sectorInfo_,
                   word ID_, Addressing addressing_,
                   const DeviceConfig& config, Load load)
	: motherBoard(config.getMotherBoard())
	, sectorInfo(std::move(sectorInfo_))
	, size(sum(sectorInfo, &SectorInfo::size))
	, ID(ID_)
	, addressing(addressing_)
{
	init(rom.getName() + "_flash", config, load, &rom);
}

AmdFlash::AmdFlash(const string& name, span<const SectorInfo> sectorInfo_,
                   word ID_, Addressing addressing_,
                   const DeviceConfig& config)
	: motherBoard(config.getMotherBoard())
	, sectorInfo(std::move(sectorInfo_))
	, size(sum(sectorInfo, &SectorInfo::size))
	, ID(ID_)
	, addressing(addressing_)
{
	init(name, config, Load::NORMAL, nullptr);
}

[[nodiscard]] static bool sramEmpty(const SRAM& ram)
{
	return ranges::all_of(xrange(ram.getSize()),
	                      [&](auto i) { return ram[i] == 0xFF; });
}

void AmdFlash::init(const string& name, const DeviceConfig& config, Load load, const Rom* rom)
{
	assert(Math::ispow2(getSize()));

	auto numSectors = sectorInfo.size();

	unsigned writableSize = 0;
	unsigned readOnlySize = 0;
	writeAddress.resize(numSectors);
	for (auto i : xrange(numSectors)) {
		if (sectorInfo[i].writeProtected) {
			writeAddress[i] = -1;
			readOnlySize += sectorInfo[i].size;
		} else {
			writeAddress[i] = writableSize;
			writableSize += sectorInfo[i].size;
		}
	}
	assert((writableSize + readOnlySize) == getSize());

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
	unsigned romSize = rom ? rom->getSize() : 0;
	unsigned offset = 0;
	for (auto i : xrange(numSectors)) {
		unsigned sectorSize = sectorInfo[i].size;
		if (isSectorWritable(unsigned(i))) {
			readAddress[i] = &(*ram)[writeAddress[i]];
			if (!loaded) {
				auto* ramPtr = const_cast<byte*>(
					&(*ram)[writeAddress[i]]);
				if (offset >= romSize) {
					// completely past end of rom
					memset(ramPtr, 0xFF, sectorSize);
				} else if (offset + sectorSize >= romSize) {
					// partial overlap
					unsigned last = romSize - offset;
					unsigned missing = sectorSize - last;
					const byte* romPtr = &(*rom)[offset];
					memcpy(ramPtr, romPtr, last);
					memset(ramPtr + last, 0xFF, missing);
				} else {
					// completely before end of rom
					const byte* romPtr = &(*rom)[offset];
					memcpy(ramPtr, romPtr, sectorSize);
				}
			}
		} else {
			assert(rom); // must have rom constructor parameter
			if ((offset + sectorSize) <= romSize) {
				readAddress[i] = &(*rom)[offset];
			} else {
				readAddress[i] = nullptr;
			}
		}
		offset += sectorSize;
	}
	assert(offset == getSize());

	reset();
}

AmdFlash::~AmdFlash() = default;

AmdFlash::GetSectorInfoResult AmdFlash::getSectorInfo(unsigned address) const
{
	address &= getSize() - 1;
	auto it = sectorInfo.begin();
	unsigned sector = 0;
	while (address >= it->size) {
		address -= it->size;
		++sector;
		++it;
		assert(it != sectorInfo.end());
	}
	unsigned sectorSize = it->size;
	unsigned offset = address;
	return {sector, sectorSize, offset};
}

void AmdFlash::reset()
{
	cmdIdx = 0;
	setState(ST_IDLE);
}

void AmdFlash::setState(State newState)
{
	if (state == newState) return;
	state = newState;
	motherBoard.getCPU().invalidateAllSlotsRWCache(0x0000, 0x10000);
}

byte AmdFlash::peek(unsigned address) const
{
	auto [sector, sectorSize, offset] = getSectorInfo(address);
	if (state == ST_IDLE) {
		if (const byte* addr = readAddress[sector]) {
			return addr[offset];
		} else {
			return 0xFF;
		}
	} else {
		if (addressing == Addressing::BITS_12) {
			// convert the address to the '11 bit case'
			address >>= 1;
		}
		switch (address & 3) {
		case 0:
			return ID >> 8;
		case 1:
			return ID & 0xFF;
		case 2:
			// 1 -> write protected
			return isSectorWritable(sector) ? 0 : 1;
		case 3:
		default:
			// TODO what is this? According to this it reads as '1'
			//  http://www.msx.org/forumtopicl8329.html
			return 1;
		}
	}
}

bool AmdFlash::isSectorWritable(unsigned sector) const
{
	return vppWpPinLow && (sector == one_of(0u, 1u)) ? false : (writeAddress[sector] != -1) ;
}

byte AmdFlash::read(unsigned address) const
{
	// note: after a read we stay in the same mode
	return peek(address);
}

const byte* AmdFlash::getReadCacheLine(unsigned address) const
{
	if (state == ST_IDLE) {
		auto [sector, sectorSize, offset] = getSectorInfo(address);
		const byte* addr = readAddress[sector];
		return addr ? &addr[offset] : MSXDevice::unmappedRead;
	} else {
		return nullptr;
	}
}

void AmdFlash::write(unsigned address, byte value)
{
	assert(cmdIdx < MAX_CMD_SIZE);
	cmd[cmdIdx].addr = address;
	cmd[cmdIdx].value = value;
	++cmdIdx;
	if (checkCommandManufacturer() ||
	    checkCommandEraseSector() ||
	    checkCommandProgram() ||
	    checkCommandDoubleByteProgram() ||
	    checkCommandQuadrupleByteProgram() ||
	    checkCommandEraseChip() ||
	    checkCommandReset()) {
		// do nothing, we're still matching a command, but it is not complete yet
	} else {
		reset();
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

bool AmdFlash::checkCommandEraseSector()
{
	static constexpr byte cmdSeq[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55 };
	if (partialMatch(5, cmdSeq)) {
		if (cmdIdx < 6) return true;
		if (cmd[5].value == 0x30) {
			unsigned addr = cmd[5].addr;
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
	static constexpr byte cmdSeq[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55 };
	if (partialMatch(5, cmdSeq)) {
		if (cmdIdx < 6) return true;
		if (cmd[5].value == 0x10) {
			if (ram) ram->memset(0, 0xff, ram->getSize());
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgramHelper(unsigned numBytes, const byte* cmdSeq, size_t cmdLen)
{
	if (partialMatch(cmdLen, cmdSeq)) {
		if (cmdIdx < (cmdLen + numBytes)) return true;
		for (auto i : xrange(cmdLen, cmdLen + numBytes)) {
			unsigned addr = cmd[i].addr;
			auto [sector, sectorSize, offset] = getSectorInfo(addr);
			if (isSectorWritable(sector)) {
				unsigned ramAddr = writeAddress[sector] + offset;
				ram->write(ramAddr, (*ram)[ramAddr] & cmd[i].value);
			}
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgram()
{
	static constexpr byte cmdSeq[] = { 0xaa, 0x55, 0xa0 };
	return checkCommandProgramHelper(1, cmdSeq, std::size(cmdSeq));
}

bool AmdFlash::checkCommandDoubleByteProgram()
{
	static constexpr byte cmdSeq[] = { 0x50 };
	return checkCommandProgramHelper(2, cmdSeq, std::size(cmdSeq));
}

bool AmdFlash::checkCommandQuadrupleByteProgram()
{
	static constexpr byte cmdSeq[] = { 0x56 };
	return checkCommandProgramHelper(4, cmdSeq, std::size(cmdSeq));
}

bool AmdFlash::checkCommandManufacturer()
{
	static constexpr byte cmdSeq[] = { 0xaa, 0x55, 0x90 };
	if (partialMatch(3, cmdSeq)) {
		if (cmdIdx == 3) {
			setState(ST_IDENT);
		}
		if (cmdIdx < 4) return true;
	}
	return false;
}

bool AmdFlash::partialMatch(size_t len, const byte* dataSeq) const
{
	static constexpr unsigned addrSeq[] = { 0, 1, 0, 0, 1 };
	unsigned cmdAddr[2] = { 0x555, 0x2aa };

	assert(len <= 5);
	for (auto i : xrange(std::min(unsigned(len), cmdIdx))) {
		// convert the address to the '11 bit case'
		unsigned addr = (addressing == Addressing::BITS_12) ? cmd[i].addr >> 1 : cmd[i].addr;
		if (((addr & 0x7FF) != cmdAddr[addrSeq[i]]) ||
		    (cmd[i].value != dataSeq[i])) {
			return false;
		}
	}
	return true;
}


static constexpr std::initializer_list<enum_string<AmdFlash::State>> stateInfo = {
	{ "IDLE",  AmdFlash::ST_IDLE  },
	{ "IDENT", AmdFlash::ST_IDENT }
};
SERIALIZE_ENUM(AmdFlash::State, stateInfo);

template<typename Archive>
void AmdFlash::AmdCmd::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("address", addr,
	             "value",   value);
}

template<typename Archive>
void AmdFlash::serialize(Archive& ar, unsigned version)
{
	ar.serialize("ram",    *ram,
	             "cmd",    cmd,
	             "cmdIdx", cmdIdx,
	             "state",  state);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("vppWpPinLow", vppWpPinLow);
	}
}
INSTANTIATE_SERIALIZE_METHODS(AmdFlash);

} // namespace openmsx
