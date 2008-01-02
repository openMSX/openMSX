// $Id$

#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

// writeProtectedFlags:  i-th bit=1 -> i-th sector write-protected
AmdFlash::AmdFlash(const Rom& rom_, unsigned logSectorSize_, unsigned totalSectors,
                   unsigned writeProtectedFlags, const XMLElement& config)
	: rom(rom_)
	, logSectorSize(logSectorSize_)
	, sectorMask((1 << logSectorSize) -1)
	, size(totalSectors << logSectorSize)
	, state(ST_IDLE)
{
	unsigned numWritable = 0;
	writeAddress.resize(totalSectors);
	for (unsigned i = 0; i < totalSectors; ++i) {
		if (writeProtectedFlags & (1 << i)) {
			writeAddress[i] = -1;
		} else {
			writeAddress[i] = numWritable << logSectorSize;
			++numWritable;
		}
	}

	unsigned writableSize = numWritable << logSectorSize;
	bool loaded = false;
	if (writableSize) {
		ram.reset(new SRAM(rom.getMotherBoard(),
		                   rom.getName() + "_flash",
		                   "flash rom", writableSize, config,
		                   0, &loaded));
	}

	readAddress.resize(totalSectors);
	unsigned numRomSectors = rom.getSize() >> logSectorSize;
	for (unsigned i = 0; i < totalSectors; ++i) {
		if (writeAddress[i] != -1) {
			readAddress[i] = &(*ram)[writeAddress[i]];
			if (!loaded) {
				byte* ramPtr =
					const_cast<byte*>(&(*ram)[writeAddress[i]]);
				if (i < numRomSectors) {
					const byte* romPtr = &rom[i << logSectorSize];
					memcpy(ramPtr, romPtr, 1 << logSectorSize);
				} else {
					memset(ramPtr, 0xFF, 1 << logSectorSize);
				}
			}
		} else {
			if (i < numRomSectors) {
				readAddress[i] = &rom[i << logSectorSize];
			} else {
				readAddress[i] = NULL;
			}
		}
	}

	//switch (type) {
	//case AMD_TYPE_1:
	//	cmdAddr[0] = 0xaaa;
	//	cmdAddr[1] = 0x555;
	//	break;
	//case AMD_TYPE_2:
		cmdAddr[0] = 0x555;
		cmdAddr[1] = 0x2aa;
	//	break;
	//default:
	//	assert(false);
	//}

	reset();
}

AmdFlash::~AmdFlash()
{
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
	rom.getMotherBoard().getCPU().invalidateMemCache(0x0000, 0x10000);
}

unsigned AmdFlash::getSize() const
{
	return size;
}

byte AmdFlash::peek(unsigned address) const
{
	address &= getSize() - 1;
	unsigned sector = address >> logSectorSize;
	if (state == ST_IDLE) {
		unsigned offset = address & sectorMask;
		const byte* addr = readAddress[sector];
		if (addr) {
			return addr[offset];
		} else {
			return 0xFF;
		}
	} else {
		switch (address & 0xff) {
		case 0:
			return 0x01;
		case 1:
			return 0xa4;
		case 2:
			// 1 -> write protected
			return (writeAddress[sector] == -1) ? 1 : 0;
		default:
			return 0xFF;
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
		address &= getSize() - 1;
		unsigned sector = address >> logSectorSize;
		unsigned offset = address & sectorMask;
		const byte* addr = readAddress[sector];
		return addr ? &addr[offset] : MSXDevice::unmappedRead;
	} else {
		return NULL;
	}
}

void AmdFlash::write(unsigned address, byte value)
{
	assert(cmdIdx < MAX_CMD_SIZE);
	cmd[cmdIdx].addr = address & (getSize() - 1);
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
			unsigned sector = addr >> logSectorSize;
			if (writeAddress[sector] != -1) {
				ram->memset(writeAddress[sector],
				            0xff, 1 << logSectorSize);
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
		unsigned sector = addr >> logSectorSize;
		if (writeAddress[sector] != -1) {
			unsigned offset = addr & sectorMask;
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

	assert(len <= 5);
	unsigned n = std::min(len, cmdIdx);
	for (unsigned i = 0; i < n; ++i) {
		if (((cmd[i].addr & 0xfff) != cmdAddr[addrSeq[i]]) ||
		    (cmd[i].value != dataSeq[i])) {
			return false;
		}
	}
	return true;
}

} // namespace openmsx
