// $Id$

#include "MSXAudio.hh"
#include "MSXMotherBoard.hh"
#include "Mixer.hh"
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
	Mixer::ChannelMode mode = Mixer::MONO;
	try {
	  std::string stereomode = config->getParameter("mode");
	   if (strcmp(stereomode.c_str(), "left")==0) {
	     mode=Mixer::MONO_LEFT;
	   };
	   if (strcmp(stereomode.c_str(), "right")==0) {
	     mode=Mixer::MONO_RIGHT;
	   };
	} catch (MSXException& e) {
	  PRT_ERROR("Exception: " << e.desc);
	}
	y8950 = new Y8950(volume, time, mode);
	reset(time);
}

MSXAudio::~MSXAudio()
{
	PRT_DEBUG("Destroying an MSXAudio object");
	delete y8950;
}

void MSXAudio::reset(const EmuTime &time)
{
	y8950->reset(time);
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
		return 0;	// avoid warning
	}
}

void MSXAudio::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0xc0:
		registerLatch = value;
		break;
	case 0xc1:
		y8950->writeReg(registerLatch, value, time);
		break;
	default:
		assert(false);
	}
}
