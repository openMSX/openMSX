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
#include "xrange.hh"
#include <bit>
#include <cassert>
#include <iterator>
#include <memory>

namespace openmsx {

AmdFlash::AmdFlash(const Rom& rom, std::span<const SectorInfo> sectorInfo_,
                   uint16_t ID_, Addressing addressing_,
                   const DeviceConfig& config, Load load)
	: motherBoard(config.getMotherBoard())
	, sectorInfo(sectorInfo_)
	, sz(sum(sectorInfo, &SectorInfo::size))
	, ID(ID_)
	, addressing(addressing_)
{
	init(rom.getName() + "_flash", config, load, &rom);
}

AmdFlash::AmdFlash(const std::string& name, std::span<const SectorInfo> sectorInfo_,
                   uint16_t ID_, Addressing addressing_,
                   const DeviceConfig& config)
	: motherBoard(config.getMotherBoard())
	, sectorInfo(sectorInfo_)
	, sz(sum(sectorInfo, &SectorInfo::size))
	, ID(ID_)
	, addressing(addressing_)
{
	init(name, config, Load::NORMAL, nullptr);
}

[[nodiscard]] static bool sramEmpty(const SRAM& ram)
{
	return ranges::all_of(xrange(ram.size()),
	                      [&](auto i) { return ram[i] == 0xFF; });
}

void AmdFlash::init(const std::string& name, const DeviceConfig& config, Load load, const Rom* rom)
{
	assert(std::has_single_bit(size()));

	auto numSectors = sectorInfo.size();

	size_t writableSize = 0;
	size_t readOnlySize = 0;
	writeAddress.resize(numSectors);
	for (auto i : xrange(numSectors)) {
		if (sectorInfo[i].writeProtected) {
			writeAddress[i] = -1;
			readOnlySize += sectorInfo[i].size;
		} else {
			writeAddress[i] = narrow<ptrdiff_t>(writableSize);
			writableSize += sectorInfo[i].size;
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
	for (auto i : xrange(numSectors)) {
		auto sectorSize = sectorInfo[i].size;
		if (isSectorWritable(i)) {
			readAddress[i] = &(*ram)[writeAddress[i]];
			if (!loaded) {
				auto* ramPtr = const_cast<uint8_t*>(
					&(*ram)[writeAddress[i]]);
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
				readAddress[i] = &(*rom)[offset];
			} else {
				readAddress[i] = nullptr;
			}
		}
		offset += sectorSize;
	}
	assert(offset == size());

	reset();
}

AmdFlash::~AmdFlash() = default;

AmdFlash::GetSectorInfoResult AmdFlash::getSectorInfo(size_t address) const
{
	address &= size() - 1;
	auto it = sectorInfo.begin();
	size_t sector = 0;
	while (address >= it->size) {
		address -= it->size;
		++sector;
		++it;
		assert(it != sectorInfo.end());
	}
	auto sectorSize = it->size;
	auto offset = address;
	return {sector, sectorSize, offset};
}

void AmdFlash::reset()
{
	cmdIdx = 0;
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
	auto [sector, sectorSize, offset] = getSectorInfo(address);
	if (state == State::IDLE) {
		if (const uint8_t* addr = readAddress[sector]) {
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
			return narrow_cast<uint8_t>(ID >> 8);
		case 1:
			return narrow_cast<uint8_t>(ID & 0xFF);
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
	static constexpr std::array<uint8_t, 5> cmdSeq = {0xaa, 0x55, 0x80, 0xaa, 0x55};
	if (partialMatch(cmdSeq)) {
		if (cmdIdx < 6) return true;
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
		if (cmdIdx < 6) return true;
		if (cmd[5].value == 0x10) {
			if (ram) ram->memset(0, 0xff, ram->size());
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgramHelper(size_t numBytes, std::span<const uint8_t> cmdSeq)
{
	if (partialMatch(cmdSeq)) {
		if (cmdIdx < (cmdSeq.size() + numBytes)) return true;
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
	return checkCommandProgramHelper(2, cmdSeq);
}

bool AmdFlash::checkCommandQuadrupleByteProgram()
{
	static constexpr std::array<uint8_t, 1> cmdSeq = {0x56};
	return checkCommandProgramHelper(4, cmdSeq);
}

bool AmdFlash::checkCommandManufacturer()
{
	static constexpr std::array<uint8_t, 3> cmdSeq = {0xaa, 0x55, 0x90};
	if (partialMatch(cmdSeq)) {
		if (cmdIdx == 3) {
			setState(State::IDENT);
		}
		if (cmdIdx < 4) return true;
	}
	return false;
}

bool AmdFlash::partialMatch(std::span<const uint8_t> dataSeq) const
{
	static constexpr std::array<unsigned, 5> addrSeq = {0, 1, 0, 0, 1};
	static constexpr std::array<unsigned, 2> cmdAddr = {0x555, 0x2aa};

	assert(dataSeq.size() <= 5);
	for (auto i : xrange(std::min(unsigned(dataSeq.size()), cmdIdx))) {
		// convert the address to the '11 bit case'
		auto addr = (addressing == Addressing::BITS_12) ? cmd[i].addr >> 1 : cmd[i].addr;
		if (((addr & 0x7FF) != cmdAddr[addrSeq[i]]) ||
		    (cmd[i].value != dataSeq[i])) {
			return false;
		}
	}
	return true;
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
