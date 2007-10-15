// $Id$

#include "AmdFlash.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

AmdFlash::AmdFlash(std::auto_ptr<Rom> rom, unsigned sectorSize_,
                   const XMLElement& config)
	: cpu(rom->getMotherBoard().getCPU())
	, sectorSize(sectorSize_)
{
	bool loaded;
	ram.reset(new SRAM(rom->getMotherBoard(), rom->getName() + "_flash",
	                  "flash rom", rom->getSize(), config, 0, &loaded));
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
	if (!loaded) {
		memcpy(const_cast<byte*>(&(*ram)[0]), &(*rom)[0], rom->getSize());
	}
	// TODO apply IPS patch

	reset();
}

AmdFlash::~AmdFlash()
{
	// TODO create IPS patch
}

void AmdFlash::reset()
{
	cmdIdx = 0;
	state = ST_IDLE;
}

unsigned AmdFlash::getSize() const
{
	return ram->getSize();
}

byte AmdFlash::peek(unsigned address) const
{
	if (state == ST_IDLE) {
		return (*ram)[address & (ram->getSize() - 1)];
	} else {
		switch (address & 0xff) {
		case 0:
			return 0x01;
		case 1:
			return 0xa4;
		}
		return 0xff;
	}
}

byte AmdFlash::read(unsigned address)
{
	byte result = peek(address);
	if (state != ST_IDLE) {
		reset();
	}
	return result;
}

const byte* AmdFlash::getReadCacheLine(unsigned address) const
{
	if (state == ST_IDLE) {
		return &(*ram)[address & (ram->getSize() - 1)];
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
			unsigned addr = cmd[5].addr & ~(sectorSize - 1);
			ram->memset(addr & (ram->getSize() - 1),
			            0xff, sectorSize);
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
			ram->memset(0, 0xff, ram->getSize());
		}
	}
	return false;
}

bool AmdFlash::checkCommandProgram()
{
	static const byte cmdSeq[] = { 0xaa, 0x55, 0xa0 };
	if (partialMatch(3, cmdSeq)) {
		if (cmdIdx < 4) return true;
		unsigned addr = cmd[3].addr & (ram->getSize() - 1);
		ram->write(addr, (*ram)[addr] & cmd[3].value);
	}
	return false;
}

bool AmdFlash::checkCommandManifacturer()
{
	static const byte cmdSeq[] = { 0xaa, 0x55, 0x90 };
	if (partialMatch(3, cmdSeq)) {
		if (cmdIdx == 3) {
			state = ST_IDENT;
			cpu.invalidateMemCache(0x0000, 0x10000);
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
