// $Id$

#include "MC6850.hh"
#include "MSXCPUInterface.hh"


MC6850::MC6850(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_Out(0x00, this);
	MSXCPUInterface::instance()->register_IO_Out(0x01, this);
	MSXCPUInterface::instance()->register_IO_In (0x04, this);
	MSXCPUInterface::instance()->register_IO_In (0x05, this);

	reset(time);
}

MC6850::~MC6850()
{
}

void MC6850::reset(const EmuTime &time)
{
}

byte MC6850::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port) {
		case 0x04:
			// read status
			result = 2;
			break;
		case 0x05:
			// read data
			result = 0;
			break;
		default:
			result = 0;	// avoid warning
	}
	PRT_DEBUG("Audio: read "<<std::hex<<(int)port<<" "<<(int)result<<std::dec);
	return result;
}

void MC6850::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0x00:
			break;
		case 0x01:
			break;
	}
}
