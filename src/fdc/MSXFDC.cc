// $Id$

#include "MSXFDC.hh"
#include "CPU.hh"


MSXFDC::MSXFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  MSXRomDevice(config, time) 
{
}

MSXFDC::~MSXFDC()
{
}

byte MSXFDC::readMem(word address, const EmuTime &time)
{
	return romBank[address & 0x3FFF];
}

byte* MSXFDC::getReadCacheLine(word start)
{
	return &romBank[start & 0x3FFF];
}
