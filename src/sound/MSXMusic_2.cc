// $Id$

#include "MSXMusic_2.hh"
#include "Mixer.hh"
#include "YM2413_2.hh"
#include "MSXConfig.hh"


MSXMusic_2::MSXMusic_2(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time), rom(config, time)
{
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	Mixer::ChannelMode mode = Mixer::MONO;
	if (config->hasParameter("mode")) {
		const std::string &stereoMode = config->getParameter("mode");
		PRT_DEBUG("mode is " << stereoMode);
		if (stereoMode == "left") mode = Mixer::MONO_LEFT;
		if (stereoMode == "right") mode = Mixer::MONO_RIGHT;
	}
	ym2413 = new YM2413_2(config->getId(), volume, time, mode);
	reset(time);
}

MSXMusic_2::~MSXMusic_2()
{
	delete ym2413;
}


void MSXMusic_2::reset(const EmuTime &time)
{
	ym2413->reset(time);
	registerLatch = 0; // TODO check
}


void MSXMusic_2::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port & 0x01) {
		case 0:
			writeRegisterPort(value, time);
			break;
		case 1:
			writeDataPort(value, time);
			break;
	}
}

void MSXMusic_2::writeRegisterPort(byte value, const EmuTime &time)
{
	registerLatch = value & 0x3F;
}

void MSXMusic_2::writeDataPort(byte value, const EmuTime &time)
{
	//PRT_DEBUG("YM2413: reg "<<(int)registerLatch<<" val "<<(int)value);
	ym2413->writeReg(registerLatch, value, time);
}


byte MSXMusic_2::readMem(word address, const EmuTime &time)
{
	return rom.read(address & 0x3FFF);
}

const byte* MSXMusic_2::getReadCacheLine(word start) const
{
	return rom.getBlock(start & 0x3FFF);
}
