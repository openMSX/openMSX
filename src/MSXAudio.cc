// $Id$

#include "MSXAudio.hh"
#include "MSXMotherBoard.hh"
#include "Y8950.hh"

MSXAudio::MSXAudio(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXAudio object");

	MSXMotherBoard::instance()->register_IO_Out(0xc0, this);
	MSXMotherBoard::instance()->register_IO_Out(0xc1, this);
	MSXMotherBoard::instance()->register_IO_In (0xc0, this);
	MSXMotherBoard::instance()->register_IO_In (0xc1, this);
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	y8950 = new Y8950(volume);
	reset(time);
}

MSXAudio::~MSXAudio()
{
	PRT_DEBUG("Destroying an MSXAudio object");
	delete y8950;
}

void MSXAudio::reset(const EmuTime &time)
{
	y8950->reset();
	registerLatch = 0;	// TODO check
}

byte MSXAudio::readIO(byte port, const EmuTime &time)
{
	switch (port) {
	case 0xc0:
		return y8950->readStatus();
	case 0xc1:
		return y8950->readReg(registerLatch);
	default:
		assert(false);
	}
}

void MSXAudio::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0xc0:
		registerLatch = value;
		break;
	case 0xc1:
		y8950->writeReg(registerLatch, value);
		break;
	default:
		assert(false);
	}
}
