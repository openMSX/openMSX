#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "Math.hh"
#include "serialize.hh"
#include "memory.hh"
#include "xrange.hh"
#include "countof.hh"
#include <numeric>
#include <cstring>
#include <cassert>

using std::vector;

namespace openmsx {

// writeProtectedFlags:  i-th bit=1 -> i-th sector write-protected
AmdFlash::AmdFlash(const Rom& rom_, const vector<SectorInfo>& sectorInfo_,
                   word ID_, bool use12bitAddressing_,
                   const DeviceConfig& config, bool load)
	: motherBoard(config.getMotherBoard())
	, rom(rom_)
	, sectorInfo(sectorInfo_)
	, size(std::accumulate(begin(sectorInfo), end(sectorInfo), 0, [](int t, SectorInfo i) { return t + i.size;}))
	, ID(ID_)
	, use12bitAddressing(use12bitAddressing_)
	, state(ST_IDLE)
	, vppWpPinLow(false)
{
	assert(Math::isPowerOfTwo(getSize()));

	auto numSectors = sectorInfo.size();

	unsigned writableSize = 0;
	writeAddress.resize(numSectors);
	for (auto i : xrange(numSectors)) {
		if (sectorInfo[i].writeProtected) {
			writeAddress[i] = -1;
		} else {
			writeAddress[i] = writableSize;
			writableSize += sectorInfo[i].size;
		}
	}

	bool loaded = false;
	if (writableSize) {
		if (load) {
			ram = make_unique<SRAM>(
				rom.getName() + "_flash", "flash rom",
				writableSize, config, nullptr, &loaded);
		} else {
			// Hack for 'Matra INK', flash chip is wired-up so that
			// writes are never visible to the MSX (but the flash
			// is not made write-protected). In this case it doesn't
			// make sense to load/save the SRAM file.
			ram = make_unique<SRAM>(
				rom.getName() + "_flash", "flash rom",
				writableSize, config, SRAM::DONT_LOAD);
		}
	}

	readAddress.resize(numSectors);
	unsigned romSize = rom.getSize();
	unsigned offset = 0;
	for (auto i : xrange(numSectors)) {
		unsigned sectorSize = sectorInfo[i].size;
		if (isSectorWritable(unsigned(i))) {
			readAddress[i] = &(*ram)[writeAddress[i]];
			if (!loaded) {
				auto ramPtr = const_cast<byte*>(
					&(*ram)[writeAddress[i]]);
				if (offset >= romSize) {
					// completely past end of rom
					memset(ramPtr, 0xFF, sectorSize);
				} else if (offset + sectorSize >= romSize) {
					// partial overlap
					unsigned last = romSize - offset;
					unsigned missing = sectorSize - last;
					const byte* romPtr = &rom[offset];
					memcpy(ramPtr, romPtr, last);
					memset(ramPtr + last, 0xFF, missing);
				} else {
					// completely before end of rom
					const byte* romPtr = &rom[offset];
					memcpy(ramPtr, romPtr, sectorSize);
				}
			}
		} else {
			if ((offset + sectorSize) <= romSize) {
				readAddress[i] = &rom[offset];
			} else {
				readAddress[i] = nullptr;
			}
		}
		offset += sectorSize;
	}
	assert(offset == getSize());

	reset();
}

AmdFlash::~AmdFlash()
{
}

void AmdFlash::getSectorInfo(unsigned address, unsigned& sector,
                             unsigned& sectorSize, unsigned& offset) const
{
	address &= getSize() - 1;
	auto it = begin(sectorInfo);
	sector = 0;
	while (address >= it->size) {
		address -= it->size;
		++sector;
		++it;
		assert(it != end(sectorInfo));
	}
	sectorSize = it->size;
	offset = address;
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
	motherBoard.getCPU().invalidateMemCache(0x0000, 0x10000);
}

byte AmdFlash::peek(unsigned address) const
{
	unsigned sector, sectorSize, offset;
	getSectorInfo(address, sector, sectorSize, offset);
	if (state == ST_IDLE) {
		if (const byte* addr = readAddress[sector]) {
			return addr[offset];
		} else {
			return 0xFF;
		}
	} else {
		if (use12bitAddressing) {
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
	return vppWpPinLow && (sector == 0 || sector == 1) ? false : (writeAddress[sector] != -1) ;
}

byte AmdFlash::read(unsigned address)
{
	// note: after a read we stay in the same mode
	return peek(address);
}

const byte* AmdFlash::getReadCacheLine(unsigned address) const
{
	if (state == ST_IDLE) {
		unsigned sector, sectorSize, offset;
		getSectorInfo(address, sector, sectorSize, offset);
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
	if (checkCommandManifacturer() ||
	    checkCommandEraseSector() ||
	    checkCommandProgram() ||
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
	static const byte cmdSeq[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55 };
	if (partialMatch(5, cmdSeq)) {
		if (cmdIdx < 6) return true;
		if (cmd[5].value == 0x30) {
			unsigned addr = cmd[5].addr;
			unsigned sector, sectorSize, offset;
			getSectorInfo(addr, sector, sectorSize, offset);
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
	static const byte cmdSeq[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55 };
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
		for (auto i = cmdLen; i < (cmdLen + numBytes); ++i) {
			unsigned addr = cmd[i].addr;
			unsigned sector, sectorSize, offset;
			getSectorInfo(addr, sector, sectorSize, offset);
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
	static const byte cmdSeq[] = { 0xaa, 0x55, 0xa0 };
	return checkCommandProgramHelper(1, cmdSeq, countof(cmdSeq));
}

bool AmdFlash::checkCommandQuadrupleByteProgram()
{
	static const byte cmdSeq[] = { 0x56 };
	return checkCommandProgramHelper(4, cmdSeq, countof(cmdSeq));
}

bool AmdFlash::checkCommandManifacturer()
{
	static const byte cmdSeq[] = { 0xaa, 0x55, 0x90 };
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
	static const unsigned addrSeq[] = { 0, 1, 0, 0, 1 };
	unsigned cmdAddr[2] = { 0x555, 0x2aa };

	assert(len <= 5);
	unsigned n = std::min(unsigned(len), cmdIdx);
	for (unsigned i = 0; i < n; ++i) {
		// convert the address to the '11 bit case'
		unsigned addr = use12bitAddressing ? cmd[i].addr >> 1 : cmd[i].addr;
		if (((addr & 0x7FF) != cmdAddr[addrSeq[i]]) ||
		    (cmd[i].value != dataSeq[i])) {
			return false;
		}
	}
	return true;
}


static std::initializer_list<enum_string<AmdFlash::State>> stateInfo = {
	{ "IDLE",  AmdFlash::ST_IDLE  },
	{ "IDENT", AmdFlash::ST_IDENT }
};
SERIALIZE_ENUM(AmdFlash::State, stateInfo);

template<typename Archive>
void AmdFlash::AmdCmd::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("address", addr);
	ar.serialize("value", value);
}

template<typename Archive>
void AmdFlash::serialize(Archive& ar, unsigned version)
{
	ar.serialize("ram", *ram);
	ar.serialize("cmd", cmd);
	ar.serialize("cmdIdx", cmdIdx);
	ar.serialize("state", state);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("vppWpPinLow", vppWpPinLow);
	}
}
INSTANTIATE_SERIALIZE_METHODS(AmdFlash);

} // namespace openmsx
