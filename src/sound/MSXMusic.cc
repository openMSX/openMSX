// $Id$

#include "MSXMusic.hh"
#include "Mixer.hh"
#include "YM2413.hh"
#include "YM2413_2.hh"
#include "Device.hh"

namespace openmsx {

MSXMusic::MSXMusic(Device* config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time), rom(config, time)
{
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	Mixer::ChannelMode mode = Mixer::MONO;
	if (config->hasParameter("mode")) {
		const string &stereoMode = config->getParameter("mode");
		PRT_DEBUG("mode is " << stereoMode);
		if (stereoMode == "left") mode = Mixer::MONO_LEFT;
		if (stereoMode == "right") mode = Mixer::MONO_RIGHT;
	}
	if (config->getParameterAsBool("alternative", false)) {
		ym2413 = new YM2413_2(config->getId(), volume, time, mode);
	} else {
		ym2413 = new YM2413(config->getId(), volume, time, mode);
	}
		
	reset(time);
}

MSXMusic::~MSXMusic()
{
	delete ym2413;
}


void MSXMusic::reset(const EmuTime& time)
{
	ym2413->reset(time);
	registerLatch = 0; // TODO check
}


void MSXMusic::writeIO(byte port, byte value, const EmuTime& time)
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

void MSXMusic::writeRegisterPort(byte value, const EmuTime& time)
{
	registerLatch = value & 0x3F;
}

void MSXMusic::writeDataPort(byte value, const EmuTime& time)
{
	//PRT_DEBUG("YM2413: reg "<<(int)registerLatch<<" val "<<(int)value);
	ym2413->writeReg(registerLatch, value, time);
}


byte MSXMusic::readMem(word address, const EmuTime& time)
{
	return rom.read(address & 0x3FFF);
}

const byte* MSXMusic::getReadCacheLine(word start) const
{
	return rom.getBlock(start & 0x3FFF);
}

} // namespace openmsx
