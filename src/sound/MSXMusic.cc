// $Id$

#include "MSXMusic.hh"
#include "Mixer.hh"
#include "YM2413.hh"
#include "YM2413_2.hh"
#include "xmlx.hh"

namespace openmsx {

MSXMusic::MSXMusic(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time), rom(getName() + " ROM", "rom", config)
{
	short volume = config.getChildDataAsInt("volume");
	
	string modeStr = config.getChildData("mode", "mono");
	Mixer::ChannelMode mode;
	if (modeStr == "left") {
		mode = Mixer::MONO_LEFT;
	} else if (modeStr == "right") {
		mode = Mixer::MONO_RIGHT;
	} else {
		mode = Mixer::MONO;
	}
	
	if (config.getChildDataAsBool("alternative", false)) {
		ym2413.reset(new YM2413(getName(), volume, time, mode));
	} else {
		ym2413.reset(new YM2413_2(getName(), volume, time, mode));
	}
	
	reset(time);
}

MSXMusic::~MSXMusic()
{
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

void MSXMusic::writeRegisterPort(byte value, const EmuTime& /*time*/)
{
	registerLatch = value & 0x3F;
}

void MSXMusic::writeDataPort(byte value, const EmuTime& time)
{
	//PRT_DEBUG("YM2413: reg "<<(int)registerLatch<<" val "<<(int)value);
	ym2413->writeReg(registerLatch, value, time);
}


byte MSXMusic::readMem(word address, const EmuTime& /*time*/)
{
	return rom[address & 0x3FFF];
}

const byte* MSXMusic::getReadCacheLine(word start) const
{
	return &rom[start & 0x3FFF];
}

} // namespace openmsx
