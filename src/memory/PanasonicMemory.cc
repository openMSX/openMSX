// $Id$

#include "PanasonicMemory.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


PanasonicMemory::PanasonicMemory()
	: rom(NULL), ram(NULL), dram(false)
{
}

PanasonicMemory::~PanasonicMemory()
{
}

PanasonicMemory* PanasonicMemory::instance()
{
	static PanasonicMemory oneInstance;
	return &oneInstance;
}

void PanasonicMemory::registerRom(const byte* rom_, int romSize_)
{
	rom = rom_;
	romSize = romSize_;
}

void PanasonicMemory::registerRam(byte* ram_, int ramSize_)
{
	ram = ram_;
	ramSize = ramSize_;
}

const byte* PanasonicMemory::getRomBlock(int block)
{
	if (dram &&
	    (((0x28 <= block) && (block < 0x2C)) ||
	     ((0x38 <= block) && (block < 0x3C)))) {
		assert(ram);
		int offset = (block & 0x03) * 0x2000;
		int ramOffset = (block < 0x30) ? ramSize - 0x10000 :
		                                 ramSize - 0x08000;
		return ram + ramOffset + offset;
	} else {
		assert(rom);
		int offset = block * 0x2000;
		if (offset >= romSize) {
			offset &= romSize - 1;
		}
		return rom + offset;
	}
}

byte* PanasonicMemory::getRamBlock(int block)
{
	assert(ram);
	int offset = block * 0x2000;
	if (offset >= ramSize) {
		offset &= ramSize - 1;
	}
	return ram + offset;
}

void PanasonicMemory::setDRAM(bool dram_)
{
	if (dram_ != dram) {
		dram = dram_;
		MSXCPU::instance()->
			invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
	}
}
