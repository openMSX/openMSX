// $Id$

#include "LedStatus.hh"
#include "MSXMotherBoard.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "BooleanSetting.hh"
#include "ReadOnlySetting.hh"

namespace openmsx {

LedStatus::LedStatus(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		std::string name = LedEvent::getLedName(static_cast<LedEvent::Led>(i));
		ledStatus[i].reset(new ReadOnlySetting<BooleanSetting>(
			motherBoard.getCommandController(),
			"led_" + name,
			"Current status for LED: " + name,
			false));
	}
}

LedStatus::~LedStatus()
{
}

void LedStatus::setLed(LedEvent::Led led, bool status)
{
	if (ledStatus[led]->getValue() == status) return;
	ledStatus[led]->setReadOnlyValue(status);

	motherBoard.getEventDistributor().distributeEvent(
		new LedEvent(led, status, motherBoard));

	static const std::string ON  = "on";
	static const std::string OFF = "off";
	motherBoard.getMSXCliComm().update(
		CliComm::LED, LedEvent::getLedName(led),
		status ? ON : OFF);
}

} // namespace openmsx
