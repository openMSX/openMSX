// $Id$

#include "MSXYM2413.hh"

#include "Mixer.hh"
#include "YM2413.hh"
#include "MSXConfig.hh"


MSXYM2413::MSXYM2413(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time), rom(config, time)
{
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	Mixer::ChannelMode mode = Mixer::MONO;
	if (config->hasParameter("mode")) {
		std::string stereomode = config->getParameter("mode");
		PRT_DEBUG("mode is " << stereomode);
		if (stereomode == "left") mode = Mixer::MONO_LEFT;
		if (stereomode == "right") mode = Mixer::MONO_RIGHT;
	}
	ym2413 = new YM2413(volume, time, mode);
	reset(time);
}

MSXYM2413::~MSXYM2413()
{
	delete ym2413;
}

void MSXYM2413::reset(const EmuTime &time)
{
	ym2413->reset(time);
	registerLatch = 0; // TODO check
	enable = 0x01; 	// TODO check (sandstone doesn't work with 0x00)
}

void MSXYM2413::writeIO(byte port, byte value, const EmuTime &time)
{
	if (enable & 0x01) {
		switch (port & 0x01) {
		case 0:
			writeRegisterPort(value, time);
			break;
		case 1:
			writeDataPort(value, time);
			break;
		}
	}
}

void MSXYM2413::writeRegisterPort(byte value, const EmuTime &time)
{
	registerLatch = value & 0x3F;
}

void MSXYM2413::writeDataPort(byte value, const EmuTime &time)
{
	//PRT_DEBUG("YM2413: reg "<<(int)registerLatch<<" val "<<(int)value);
	Mixer::instance()->updateStream(time);
	ym2413->writeReg(registerLatch, value, time);
}
