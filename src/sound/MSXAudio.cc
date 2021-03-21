#include "MSXAudio.hh"
#include "Y8950Periphery.hh"
#include "DACSound8U.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <memory>

using std::string;

namespace openmsx {

// MSXAudio

MSXAudio::MSXAudio(const DeviceConfig& config)
	: MSXDevice(config)
	, y8950(getName(), config,
	        config.getChildDataAsInt("sampleram", 256) * 1024,
	        getCurrentTime(), *this)
	, dacValue(0x80), dacEnabled(false)
{
	auto type = config.getChildData("type", "philips");
	StringOp::casecmp cmp;
	if (cmp(type, "philips")) {
		dac = std::make_unique<DACSound8U>(
			getName() + " 8-bit DAC", "MSX-AUDIO 8-bit DAC",
			config);
	}
	powerUp(getCurrentTime());
}

MSXAudio::~MSXAudio()
{
	// delete soon, because PanasonicAudioPeriphery still uses
	// this object in its destructor
	periphery.reset();
}

Y8950Periphery& MSXAudio::createPeriphery(const string& soundDeviceName)
{
	periphery = Y8950PeripheryFactory::create(
		*this, getDeviceConfig2(), soundDeviceName);
	return *periphery;
}

void MSXAudio::powerUp(EmuTime::param time)
{
	y8950.clearRam();
	reset(time);
}

void MSXAudio::reset(EmuTime::param time)
{
	y8950.reset(time);
	periphery->reset();
	registerLatch = 0; // TODO check
}

byte MSXAudio::readIO(word port, EmuTime::param time)
{
	if ((port & 0xE8) == 0x08) {
		// read DAC
		return 0xFF;
	} else {
		return (port & 1) ? y8950.readReg(registerLatch, time)
		                  : y8950.readStatus(time);
	}
}

byte MSXAudio::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xE8) == 0x08) {
		// read DAC
		return 0xFF; // read always returns 0xFF
	} else {
		return (port & 1) ? y8950.peekReg(registerLatch, time)
		                  : y8950.peekStatus(time);
	}
}

void MSXAudio::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xE8) == 0x08) {
		dacValue = value;
		if (dacEnabled) {
			assert(dac);
			dac->writeDAC(dacValue, time);
		}
	} else if ((port & 0x01) == 0) {
		// 0xC0 or 0xC2
		registerLatch = value;
	} else {
		// 0xC1 or 0xC3
		y8950.writeReg(registerLatch, value, time);
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
	if ((dacEnabled != enable) && dac) {
		dacEnabled = enable;
		byte value = dacEnabled ? dacValue : 0x80;
		dac->writeDAC(value, time);
	}
}

template<typename Archive>
void MSXAudio::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serializePolymorphic("periphery", *periphery);
	ar.serialize("Y8950",         y8950,
	             "registerLatch", registerLatch,
	             "dacValue",      dacValue,
	             "dacEnabled",    dacEnabled);

	if constexpr (Archive::IS_LOADER) {
		// restore dac status
		if (dacEnabled) {
			assert(dac);
			dac->writeDAC(dacValue, getCurrentTime());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXAudio);
REGISTER_MSXDEVICE(MSXAudio, "MSX-Audio");

} // namespace openmsx
