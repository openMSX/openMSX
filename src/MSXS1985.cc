// $Id$

#include "MSXS1985.hh"
#include "MSXCPUInterface.hh"


MSXS1985::MSXS1985(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In (0x40, this);
	MSXCPUInterface::instance()->register_IO_Out(0x40, this);
	MSXCPUInterface::instance()->register_IO_Out(0x41, this);
	MSXCPUInterface::instance()->register_IO_In (0x42, this);
	MSXCPUInterface::instance()->register_IO_Out(0x42, this);
	
	MSXCPUInterface::instance()->register_IO_Out(0x46, this);
	MSXCPUInterface::instance()->register_IO_In (0x47, this);
	MSXCPUInterface::instance()->register_IO_Out(0x47, this);

	// TODO load ram
}

MSXS1985::~MSXS1985()
{
	// TODO save ram
}


void MSXS1985::reset(const EmuTime &time)
{
}

byte MSXS1985::readIO(byte port, const EmuTime &time)
{
	switch (port) {
		case 0x40:
			// TODO In every case?
			return 0x01;
		case 0x42:
			return ram[address];
		case 0x47: {
			byte result = (pattern & 0x80) ? color2 : color1;
			pattern = (pattern << 1) | (pattern >> 7);
			return result;
		}
		default:
			assert(false);
			return 255;	// avoid warning
	}
}

void MSXS1985::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0x40:
			// TODO You should write 0xFE to this port,
			//      but why exactly?
			break;
		case 0x41:
			address = value & 0x0F;
			break;
		case 0x42:
			ram[address] = value;
			break;
		case 0x46:
			color2 = color1;
			color1 = value;
			break;
		case 0x47:
			pattern = value;
			break;
	}
}
