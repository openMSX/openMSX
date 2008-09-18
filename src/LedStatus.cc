// $Id$

#include "LedStatus.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"
#include "BooleanSetting.hh"
#include "ReadOnlySetting.hh"

namespace openmsx {

static std::string getLedName(LedStatus::Led led)
{
	static const char* const names[LedStatus::NUM_LEDS] = {
		"power", "caps", "kana", "pause", "turbo", "FDD"
	};
	return names[led];
}

LedStatus::LedStatus(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	for (int i = 0; i < NUM_LEDS; ++i) {
		std::string name = getLedName(static_cast<Led>(i));
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

void LedStatus::setLed(Led led, bool status)
{
	if (ledStatus[led]->getValue() == status) return;
	ledStatus[led]->setReadOnlyValue(status);

	static const std::string ON  = "on";
	static const std::string OFF = "off";
	motherBoard.getMSXCliComm().update(
		CliComm::LED, getLedName(led),
		status ? ON : OFF);
}

} // namespace openmsx
