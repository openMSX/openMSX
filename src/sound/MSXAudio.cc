// $Id$

#include "MSXAudio.hh"
#include "Y8950.hh"
#include "Y8950Periphery.hh"
#include "DACSound8U.hh"
#include "MSXMotherBoard.hh"
#include "StringOp.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include <string>

using std::string;

namespace openmsx {

// MSXAudio

MSXAudio::MSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, dacValue(0x80), dacEnabled(false)
{
	periphery.reset(Y8950PeripheryFactory::create(*this, config));
	string type = StringOp::toLower(config.getChildData("type", "philips"));
	if (type == "philips") {
		dac.reset(new DACSound8U(motherBoard.getMSXMixer(),
		                         getName() + " 8-bit DAC", "MSX-AUDIO 8-bit DAC",
		                         config));
	}
	int ramSize = config.getChildDataAsInt("sampleram", 256); // size in kb
	EmuTime::param time = getCurrentTime();
	y8950.reset(new Y8950(motherBoard, getName(), config, ramSize * 1024,
	                      time, *periphery));
	powerUp(time);
}

MSXAudio::~MSXAudio()
{
	// delete soon, because PanasonicAudioPeriphery still uses
	// this object in its destructor
	periphery.reset();
}

void MSXAudio::powerUp(EmuTime::param time)
{
	y8950->clearRam();
	reset(time);
}

void MSXAudio::reset(EmuTime::param time)
{
	y8950->reset(time);
	periphery->reset();
	registerLatch = 0; // TODO check
}

byte MSXAudio::readIO(word port, EmuTime::param time)
{
	byte result;
	if ((port & 0xFF) == 0x0A) {
		// read DAC
		result = 255;
	} else {
		result = (port & 1) ? y8950->readReg(registerLatch, time)
		                    : y8950->readStatus();
	}
	//std::cout << "read:  " << (int)(port& 0xff) << " " << (int)result << std::endl;
	return result;
}

byte MSXAudio::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xFF) == 0x0A) {
		// read DAC
		return 255; // read always returns 255
	} else {
		return (port & 1) ? y8950->peekReg(registerLatch, time)
		                  : y8950->peekStatus();
	}
}

void MSXAudio::writeIO(word port, byte value, EmuTime::param time)
{
	//std::cout << "write: " << (int)(port& 0xff) << " " << (int)value << std::endl;
	if ((port & 0xFF) == 0x0A) {
		dacValue = value;
		if (dacEnabled) {
			assert(dac.get());
			dac->writeDAC(dacValue, time);
		}
	} else if ((port & 0x01) == 0) {
		// 0xC0 or 0xC2
		registerLatch = value;
	} else {
		// 0xC1 or 0xC3
		y8950->writeReg(registerLatch, value, time);
	}
}

byte MSXAudio::readMem(word address, EmuTime::param time)
{
	return periphery->readMem(address, time);
}
byte MSXAudio::peekMem(word address, EmuTime::param time) const
{
	return periphery->peekMem(address, time);
}
void MSXAudio::writeMem(word address, byte value, EmuTime::param time)
{
	periphery->writeMem(address, value, time);
}
const byte* MSXAudio::getReadCacheLine(word start) const
{
	return periphery->getReadCacheLine(start);
}
byte* MSXAudio::getWriteCacheLine(word start) const
{
	return periphery->getWriteCacheLine(start);
}

void MSXAudio::enableDAC(bool enable, EmuTime::param time)
{
	if ((dacEnabled != enable) && dac.get()) {
		dacEnabled = enable;
		byte value = dacEnabled ? dacValue : 0x80;
		dac->writeDAC(value, time);
	}
}

template<typename Archive>
void MSXAudio::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serializePolymorphic("periphery", *periphery);
	ar.serialize("Y8950", *y8950);

	ar.serialize("registerLatch", registerLatch);
	ar.serialize("dacValue", dacValue);
	ar.serialize("dacEnabled", dacEnabled);

	if (ar.isLoader()) {
		// restore dac status
		if (dacEnabled) {
			assert(dac.get());
			dac->writeDAC(dacValue, getCurrentTime());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXAudio);
REGISTER_MSXDEVICE(MSXAudio, "MSX-Audio");

} // namespace openmsx
