// $Id$

/**
 * Based on code from NLMSX written by Frits Hilderink
 */

#include "TurboRFDC.hh"
#include "TC8566AF.hh"
#include "CPU.hh"
#include "MSXCPU.hh"


TurboRFDC::TurboRFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXFDC(config, time), MSXDevice(config, time)
{
	emptyRom = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRom, 255, CPU::CACHE_LINE_SIZE);
	
	controller = new TC8566AF(drives, time);
	reset(time);
}

TurboRFDC::~TurboRFDC()
{
	delete controller;
	delete[] emptyRom;
}

void TurboRFDC::reset(const EmuTime &time)
{
	memory = romBank;
	controller->reset(time);
}

byte TurboRFDC::readMem(word address, const EmuTime &time)
{
	byte result;
	if (0x3FF0 <= (address & 0x3FFF)) {
		switch (address & 0x3FFF) {
		case 0x3FF1:
			// bit 0	FD2HD1	High Density detect drive 1
			// bit 1	FD2HD2	High Density detect drive 2
			// bit 4	FDCHG1	Disk Change detect on drive 1
			// bit 5	FDCHG2	Disk Change detect on drive 2
			// active low
			result = 0x33;
			/*
			if (MSXIOctxDiskGet(Device->Disk[0])->Changed)
				result &= ~0x10;
			if (MSXIOctxDiskGet(Device->Disk[1])->Changed)
				result &= ~0x20;
			MSXIOctxDiskGet(Device->Disk[0])->Changed = false;
			MSXIOctxDiskGet(Device->Disk[1])->Changed = false;
			*/
			break;
		case 0x3FF4:
		case 0x3FFA:
			result = controller->readReg(4, time);
			break;
		case 0x3FF5:
		case 0x3FFB:
			result = controller->readReg(5, time);
			break;
		default:
			result = 0xFF;
			break;
		}
	} else if ((0x4000 <= address) && (address < 0x8000)) {
		result = memory[address & 0x3FFF];
	} else {
		result = 0xFF;
	}
	PRT_DEBUG("TurboRFDC: read 0x" << std::hex << (int)address << " 0x" << (int)result << std::dec);
	return result;
}

byte* TurboRFDC::getReadCacheLine(word start)
{
	if ((start & 0x3FF0 & CPU::CACHE_LINE_HIGH) == (0x3FF0 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else if ((0x4000 <= start) && (start < 0x8000)) {
		return &memory[start & 0x3FFF];
	} else {
		return emptyRom;
	}
}

void TurboRFDC::writeMem(word address, byte value, const EmuTime &time)  
{
	PRT_DEBUG("TurboRFDC: write 0x" << std::hex << (int)address << " 0x" << (int)value << std::dec);
	if ((address == 0x6000) || (address == 0x7FF0) || (address == 0x7FFE)) {
		MSXCPU::instance()->invalidateCache(0x4000, 0x4000/CPU::CACHE_LINE_SIZE);
		memory = romBank + (0x4000 * (value & 0x03));
		return;
	} else {
		switch (address & 0x3FFF) {
		case 0x3ff2:
		case 0x3ff8:
			// bit 0	Drive select bit 0
			// bit 1	Drive select bit 1
			// bit 2	0 = Reset FDC, 1 = Enable FDC
			// bit 3	1 = Enable DMA and interrupt, 0 = always '0' on Turbo R
			// bit 4	Motor Select Drive A
			// bit 5	Motor Select Drive B
			// Bit 6	Motor Select Drive C
			// Bit 7	Motor Select Drive D
			controller->writeReg(2, value, time);
			break;
		case 0x3ff3:
		case 0x3ff9:
			controller->writeReg(3, value, time);
			break;
		case 0x3ff4:
		case 0x3ffa:
			controller->writeReg(4, value, time);
			break;
		case 0x3ff5:
		case 0x3ffb:	// FDC data port
			controller->writeReg(5, value, time);
			break;
		}
	}
}
