// $Id$

#include "MSXTurboRPause.hh"
#include "MSXCPUInterface.hh"


MSXTurboRPause::MSXTurboRPause(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In(0xA7, this);
	reset(time);
}

MSXTurboRPause::~MSXTurboRPause()
{
}
 
void MSXTurboRPause::reset(const EmuTime &time)
{
	status = 0;
}

byte MSXTurboRPause::readIO(byte port, const EmuTime &time)
{
	return status;
}


// TODO implement "turborpause" command
