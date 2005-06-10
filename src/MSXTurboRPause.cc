// $Id$

#include "LedEvent.hh"
#include "EventDistributor.hh"
#include "MSXTurboRPause.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

MSXTurboRPause::MSXTurboRPause(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, pauseSetting("turborpause", "status of the TurboR pause", false)
	, status(255)
	, pauseLed(false)
	, turboLed(false)
	, hwPause(false)
{
	pauseSetting.addListener(this);
	reset(time);
}

MSXTurboRPause::~MSXTurboRPause()
{
	pauseSetting.removeListener(this);
}

void MSXTurboRPause::reset(const EmuTime& time)
{
	pauseSetting.setValue(false);
	writeIO(0, 0, time);
}

void MSXTurboRPause::powerDown(const EmuTime& time)
{
	writeIO(0, 0, time);
}

byte MSXTurboRPause::readIO(byte port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXTurboRPause::peekIO(byte /*port*/, const EmuTime& /*time*/) const
{
	return pauseSetting.getValue() ? 1 : 0;
}

void MSXTurboRPause::writeIO(byte /*port*/, byte value, const EmuTime& /*time*/)
{
	status = value;
	bool newTurboLed = (status & 0x80);
	if (newTurboLed != turboLed) {
		turboLed = newTurboLed;
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::TURBO, turboLed));
	}
	updatePause();
}

void MSXTurboRPause::update(const Setting* /*setting*/)
{
	updatePause();
}

void MSXTurboRPause::updatePause()
{
	bool newHwPause = (status & 0x02) && pauseSetting.getValue();
	if (newHwPause != hwPause) {
		hwPause = newHwPause;
		if (hwPause) {
			getMotherboard().block();
		} else {
			getMotherboard().unblock();
		}
	}

	bool newPauseLed = (status & 0x01) || hwPause;
	if (newPauseLed != pauseLed) {
		pauseLed = newPauseLed;
		EventDistributor::instance().distributeEvent(
			new LedEvent(LedEvent::PAUSE, pauseLed));
	}
}

} // namespace openmsx
