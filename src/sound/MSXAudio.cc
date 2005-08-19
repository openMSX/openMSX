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
	y8950.reset(new Y8950(motherBoard, getName(), config, ramSize * 1024, time));
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
	return (port & 1) ? y8950->readReg(registerLatch, time)  // port 1
	                  : y8950->readStatus();                 // port 0
}

byte MSXAudio::peekIO(byte port, const EmuTime& time) const
{
	return (port & 1) ? y8950->peekReg(registerLatch, time)  // port 1
	                  : y8950->peekStatus();                 // port 0
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
