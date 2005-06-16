// $Id$

#include "MSXAudio.hh"
#include "Y8950.hh"
#include "MSXMotherBoard.hh"
#include "XMLElement.hh"

namespace openmsx {

MSXAudio::MSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
	int ramSize = config.getChildDataAsInt("sampleram", 256); // size in kb
	y8950.reset(new Y8950(getName(), config, ramSize * 1024, time,
	                      getMotherBoard().getCPU()));
	reset(time);
}

MSXAudio::~MSXAudio()
{
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

byte MSXAudio::peekIO(byte /*port*/, const EmuTime& /*time*/) const
{
	// TODO not implemented
	return 0xFF;
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
