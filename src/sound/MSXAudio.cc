// $Id$

#include "MSXAudio.hh"
#include "Mixer.hh"
#include "Y8950.hh"
#include "xmlx.hh"

namespace openmsx {

MSXAudio::MSXAudio(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	short volume = config.getChildDataAsInt("volume");

	// left / right / mono
	Mixer::ChannelMode mode = Mixer::MONO;
	const string &stereomode = config.getChildData("mode");
	if (stereomode == "left") {
		mode=Mixer::MONO_LEFT;
	} else if (stereomode == "right") {
		mode=Mixer::MONO_RIGHT;
	}

	// SampleRAM size
	int ramSize = config.getChildDataAsInt("sampleram", 256); // size in kb

	y8950 = new Y8950(getName(), volume, ramSize * 1024, time, mode);
	reset(time);
}

MSXAudio::~MSXAudio()
{
	delete y8950;
}

void MSXAudio::reset(const EmuTime& time)
{
	y8950->reset(time);
	registerLatch = 0;	// TODO check
}

byte MSXAudio::readIO(byte port, const EmuTime& time)
{
	byte result;
	switch (port & 0x01) {
	case 0:
		result = y8950->readStatus();
		break;
	case 1:
		result = y8950->readReg(registerLatch, time);
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
	//PRT_DEBUG("Audio: read "<<hex<<(int)port<<" "<<(int)result<<dec);
	return result;
}

void MSXAudio::writeIO(byte port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("Audio: write "<<hex<<(int)port<<" "<<(int)value<<dec);
	switch (port & 0x01) {
	case 0:
		registerLatch = value;
		break;
	case 1:
		y8950->writeReg(registerLatch, value, time);
		break;
	default:
		assert(false);
	}
}

} // namespace openmsx
