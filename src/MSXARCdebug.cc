// $Id$

#include "MSXARCdebug.hh"


MSXARCdebug::MSXARCdebug(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	std::string filename = config->getParameter("filename");
	file = FileOpener::openFileTruncate(filename);
}

MSXARCdebug::~MSXARCdebug()
{
}

byte MSXARCdebug::readMem(word address, const EmuTime &time)
{
	byte data = 255;
	PRT_DEBUG("MSXARCdebug::readMem(0x" << std::hex << (int)address << ").");
	*file <<"read 0x" << std::hex << (int)address << " returning" <<(int)data<<"\n";
	return data;
}

void MSXARCdebug::writeMem(word address, byte value, const EmuTime &time)
{
	*file <<"write 0x" << std::hex << (int)address << ", value "<<(int)value<<"\n";
	PRT_DEBUG("MSXARCdebug::writeMem(0x" << std::hex << (int)address << std::dec
		<< ", value "<<(int)value<<").");
}

