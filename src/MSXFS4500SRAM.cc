// $Id$

#include "MSXFS4500SRAM.hh"
#include "MSXCPUInterface.hh"


MSXFS4500SRAM::MSXFS4500SRAM(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  sram(0x800, config)
{
	// TODO find out what these ports are used for
	//      (and if they belong to this device)
	//MSXCPUInterface::instance()->register_IO_Out(0x40, this);
	//MSXCPUInterface::instance()->register_IO_In (0x41, this);
	//MSXCPUInterface::instance()->register_IO_Out(0x43, this);
	//MSXCPUInterface::instance()->register_IO_In (0x43, this);
	//MSXCPUInterface::instance()->register_IO_Out(0x44, this);
	//MSXCPUInterface::instance()->register_IO_In (0x45, this);
	//MSXCPUInterface::instance()->register_IO_In (0x46, this);
	
	MSXCPUInterface::instance()->register_IO_Out(0x47, this);
	MSXCPUInterface::instance()->register_IO_Out(0x48, this);
	MSXCPUInterface::instance()->register_IO_Out(0x49, this);
	MSXCPUInterface::instance()->register_IO_In (0x49, this);
}

MSXFS4500SRAM::~MSXFS4500SRAM()
{
}

void MSXFS4500SRAM::reset(const EmuTime &time)
{
	address = 0;	// TODO check this
}

byte MSXFS4500SRAM::readIO(byte port, const EmuTime &time)
{
	// TODO can you read from 0x47 0x48
	assert(port == 0x49);
	
	// read sram
	byte result = sram.read(address);
	address = (address + 1) & 0x7FF;
	return result;
}

void MSXFS4500SRAM::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0x47:
			// set address (low)
			address = (address & ~0xFF) | value;
			break;
		case 0x48:
			// set address (high)
			address = (address & ~0xFF00) | ((value & 0x07) << 8);
			break;
		case 0x49:
			// write sram
			sram.write(address, value);
			address = (address + 1) & 0x7FF;
			break;
		default:
			assert(false);
	}
}
