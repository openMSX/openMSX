// $Id$

#include "MC6850.hh"
#include "MSXMotherBoard.hh"


MC6850::MC6850(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating an MC6850 object");

	MSXMotherBoard::instance()->register_IO_Out(0x00, this);
	MSXMotherBoard::instance()->register_IO_Out(0x01, this);
	MSXMotherBoard::instance()->register_IO_In (0x04, this);
	MSXMotherBoard::instance()->register_IO_In (0x05, this);

	reset(time);
}

MC6850::~MC6850()
{
	PRT_DEBUG("Destroying an MC6850 object");
}

void MC6850::reset(const EmuTime &time)
{
}

byte MC6850::readIO(byte port, const EmuTime &time)
{
	switch (port) {
	case 0x04:
		// read status
		return 2;
	case 0x05:
		// read data
		return 0;
	}
	return 0;	// avoid warning
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
