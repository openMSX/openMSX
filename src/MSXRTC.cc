// $Id$

#include "MSXRTC.hh"
#include "RP5C01.hh"
#include "File.hh"
#include "SettingsConfig.hh"
#include "Config.hh"

namespace openmsx {

MSXRTC::MSXRTC(Config* config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  sram(getName() + " SRAM", 4 * 13, config),
	  settingsConfig(SettingsConfig::instance())
{
	bool emuTimeBased = true;
	
	try {
		Config* config = settingsConfig.getConfigById("RTC");
		string mode = config->getParameter("mode", "EmuTime"); 
		emuTimeBased = (mode == "EmuTime");
	} catch (ConfigException &e) {
		// No RTC section
	}
	
	rp5c01 = new RP5C01(emuTimeBased, &sram[0], time);
	registerLatch = 0; // Note: not verified on real hardware.
}

MSXRTC::~MSXRTC()
{
	delete rp5c01;
}

void MSXRTC::reset(const EmuTime& time)
{
	rp5c01->reset(time);
}

byte MSXRTC::readIO(byte port, const EmuTime& time)
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

