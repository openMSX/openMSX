// $Id$

#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "Math.hh"
#include "serialize.hh"
#include <numeric>
#include <cstring>
#include <cassert>

using std::vector;

namespace openmsx {

// writeProtectedFlags:  i-th bit=1 -> i-th sector write-protected
AmdFlash::AmdFlash(MSXMotherBoard& motherBoard_, const Rom& rom_,
                   const vector<unsigned>& sectorSizes_,
                   unsigned writeProtectedFlags, word ID_,
                   const XMLElement& config)
	: motherBoard(motherBoard_)
	, rom(rom_)
	, sectorSizes(sectorSizes_)
	, size(std::accumulate(sectorSizes.begin(), sectorSizes.end(), 0))
	, ID(ID_)
	, state(ST_IDLE)
{
	init(writeProtectedFlags, &config);
}

AmdFlash::AmdFlash(MSXMotherBoard& motherBoard_, const Rom& rom_,
                   const vector<unsigned>& sectorSizes_,
                   unsigned writeProtectedFlags, word ID_)
	: motherBoard(motherBoard_)
	, rom(rom_)
	, sectorSizes(sectorSizes_)
	, size(std::accumulate(sectorSizes.begin(), sectorSizes.end(), 0))
	, ID(ID_)
	, state(ST_IDLE)
{
	init(writeProtectedFlags, NULL); // don't load/save
}

void AmdFlash::init(unsigned writeProtectedFlags, const XMLElement* config)
{
	assert(Math::isPowerOfTwo(getSize()));

	unsigned numSectors = sectorSizes.size();

	unsigned writableSize = 0;
	writeAddress.resize(numSectors);
	for (unsigned i = 0; i < numSectors; ++i) {
		if (writeProtectedFlags & (1 << i)) {
			writeAddress[i] = -1;
		} else {
			writeAddress[i] = writableSize;
			writableSize += sectorSizes[i];
		}
	}

	bool loaded = false;
	if (writableSize) {
		if (config) {
			ram.reset(new SRAM(motherBoard, rom.getName() + "_flash",
			                   "flash rom", writableSize, *config,
			                   0, &loaded));
		} else {
			// Hack for 'Matra INK', flash chip is wired-up so that
			// writes are never visible to the MSX (but the flash
			// is not made write-protected). In this case it doesn't
			// make sense to load/save the SRAM file.
			ram.reset(new SRAM(motherBoard, rom.getName() + "_flash",
			                   "flash rom", writableSize));
		}
	}

	readAddress.resize(numSectors);
	unsigned romSize = rom.getSize();
	unsigned offset = 0;
	for (unsigned i = 0; i < numSectors; ++i) {
		unsigned sectorSize = sectorSizes[i];
		if (writeAddress[i] != -1) {
			readAddress[i] = &(*ram)[writeAddress[i]];
			if (!loaded) {
				byte* ramPtr =
					const_cast<byte*>(&(*ram)[writeAddress[i]]);
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
			if ((offset + sectorSize) < romSize) {
				readAddress[i] = &rom[offset];
			} else {
				readAddress[i] = NULL;
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
	std::vector<unsigned>::const_iterator it = sectorSizes.begin();
	sector = 0;
	while (address >= *it) {
		address -= *it;
		++sector;
		++it;
		assert(it != sectorSizes.end());
	}
	sectorSize = *it;
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

unsigned AmdFlash::getSize() const
{
	return size;
}

byte AmdFlash::peek(unsigned address) const
{
	unsigned sector, sectorSize, offset;
	getSectorInfo(address, sector, sectorSize, offset);
	if (state == ST_IDLE) {
		const byte* addr = readAddress[sector];
		if (addr) {
			return addr[offset];
		} else {
			return 0xFF;
		}
	} else {
		switch (address & 3) {
		case 0:
			return ID >> 8;
		case 1:
			return ID & 0xFF;
		case 2:
			// 1 -> write protected
			return (writeAddress[sector] == -1) ? 1 : 0;
		case 3:
		default:
			// TODO what is this? According to this it reads as '1'
			//  http://www.msx.org/forumtopicl8329.html
			return 1;
		}
	}
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
		return NULL;
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
	    checkCommandEraseChip()) {
		if (value == 0xf0) {
			reset();
		}
	} else {
		reset();
	}
}

// The 4 checkCommandXXX() methods below return
//   true  -> if the command sequence still matches, but is not complete yet
//   false -> if the command was fully matched or does not match with
//            the current command sequence.
// If there was a full match, the command is also executed.
bool AmdFlash::checkCommandEraseSector()
{
	static const byte cmdSeq[] = { 0xaa, 0x55, 0x80, 0xaa, 0x55 };
	if (partialMatch(5, cmdSeq)) {
		if (cmdIdx < 6) return true;
		if (cmd[5].value == 0x30) {
			unsigned addr = cmd[5].addr;
			unsigned sector, sectorSize, offset;
			getSectorInfo(addr, sector, sectorSize, offset);
			if (writeAddress[sector] != -1) {
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
			if (ram.get()) {
				ram->memset(0, 0xff, ram->getSize());
			}
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgram()
{
	static const byte cmdSeq[] = { 0xaa, 0x55, 0xa0 };
	if (partialMatch(3, cmdSeq)) {
		if (cmdIdx < 4) return true;
		unsigned addr = cmd[3].addr;
		unsigned sector, sectorSize, offset;
		getSectorInfo(addr, sector, sectorSize, offset);
		if (writeAddress[sector] != -1) {
			unsigned ramAddr = writeAddress[sector] + offset;
			ram->write(ramAddr, (*ram)[ramAddr] & cmd[3].value);
		}
	}
	return false;
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

bool AmdFlash::partialMatch(unsigned len, const byte* dataSeq) const
{
	static const unsigned addrSeq[] = { 0, 1, 0, 0, 1 };
	unsigned cmdAddr[2] = { 0x555, 0x2aa };

	assert(len <= 5);
	unsigned n = std::min(len, cmdIdx);
	for (unsigned i = 0; i < n; ++i) {
		if (((cmd[i].addr & 0x7ff) != cmdAddr[addrSeq[i]]) ||
		    (cmd[i].value != dataSeq[i])) {
			return false;
		}
	}
	return true;
}


static enum_string<AmdFlash::State> stateInfo[] = {
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
void AmdFlash::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ram", *ram);
	ar.serialize("cmd", cmd);
	ar.serialize("cmdIdx", cmdIdx);
	ar.serialize("state", state);
}
INSTANTIATE_SERIALIZE_METHODS(AmdFlash);

} // namespace openmsx
