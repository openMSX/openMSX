// $Id$

#include "MSXRam.hh"
#include "CPU.hh"


MSXRam::MSXRam(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	// slow drain on reset
	if (deviceConfig->hasParameter("slow_drain_on_reset")) {
		slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	} else {
		slowDrainOnReset = false;
	}

	// size / end
	int s;
	if (deviceConfig->hasParameter("size")) {
		s = deviceConfig->getParameterAsInt("size");
		if ((s <= 0) || (s > 64)) {
			PRT_ERROR("Wrong RAM size");
		}
	} else {
		s = 64; 
	}

	// base
	if (deviceConfig->hasParameter("base")) {
		int b = deviceConfig->getParameterAsInt("base");
		if ((b < 0) || ((b + s) > 64)) {
			PRT_ERROR("Wrong RAM base");
		}
		base = b * 1024;
	} else {
		base = 0;
	}
	int size = s * 1024;
	end = base + size;
	
	assert(CPU::CACHE_LINE_SIZE <= 1024);	// size must be cache aligned
	
	memoryBank = new byte[size];
	memset(memoryBank, 0xFF, size);

	emptyRead = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRead, 0xFF, CPU::CACHE_LINE_SIZE);

	emptyWrite = new byte[CPU::CACHE_LINE_SIZE];
}

MSXRam::~MSXRam()
{
	delete[] memoryBank;
	delete[] emptyRead;
	delete[] emptyWrite;
}

void MSXRam::reset(const EmuTime &time)
{
	if (!slowDrainOnReset) {
		int size = end - base;
		memset(memoryBank, 0, size);
	}
}

bool MSXRam::isInside(word address) const
{
	return ((base <= address) && (address < end));
}

byte MSXRam::readMem(word address, const EmuTime &time)
{
	if (isInside(address)) {
		return memoryBank[address - base];
	} else {
		return 0xFF;
	}
}

void MSXRam::writeMem(word address, byte value, const EmuTime &time)
{
	if (isInside(address)) {
		memoryBank[address - base] = value;
	} else {
		// ignore
	}
}

const byte* MSXRam::getReadCacheLine(word start) const
{
	if (isInside(start)) {
		return &memoryBank[start - base];
	} else {
		return emptyRead;
	}
}

byte* MSXRam::getWriteCacheLine(word start) const
{
	if (isInside(start)) {
		return &memoryBank[start - base];
	} else {
		return emptyWrite;
	}
}
