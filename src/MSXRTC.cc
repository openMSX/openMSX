// $Id$

#include "MSXRTC.hh"
#include "RP5C01.hh"
#include "File.hh"
#include "SettingsConfig.hh"

namespace openmsx {

MSXRTC::MSXRTC(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  sram(getName() + " SRAM", 4 * 13, config),
	  settingsConfig(SettingsConfig::instance())
{
	bool emuTimeBased = true;
	const XMLElement* rtcConfig = settingsConfig.findConfigById("RTC");
	if (rtcConfig) {
		const XMLElement* modeParam = rtcConfig->getChild("mode");
		if (modeParam) {
			emuTimeBased = (modeParam->getData() == "EmuTime");
		}
	}
	
	rp5c01.reset(new RP5C01(emuTimeBased, &sram[0], time));
	registerLatch = 0; // TODO verify on real hardware
}

MSXRTC::~MSXRTC()
{
}

void MSXRTC::reset(const EmuTime& time)
{
	rp5c01->reset(time);
}

byte MSXRTC::readIO(byte /*port*/, const EmuTime& time)
{
	return rp5c01->readPort(registerLatch, time) | 0xF0;
}

void MSXRTC::writeIO(byte port, byte value, const EmuTime& time)
{
	switch (port & 0x01) {
	case 0:
		registerLatch = value & 0x0F;
		break;
	case 1:
		rp5c01->writePort(registerLatch, value & 0x0F, time);
		break;
	default:
		assert(false);
	}
}

} // namespace openmsx

