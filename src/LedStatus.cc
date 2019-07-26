#include "LedStatus.hh"
#include "MSXCliComm.hh"
#include "ReadOnlySetting.hh"
#include "CommandController.hh"
#include "Timer.hh"
#include <memory>

namespace openmsx {

static std::string getLedName(LedStatus::Led led)
{
	static const char* const names[LedStatus::NUM_LEDS] = {
		"power", "caps", "kana", "pause", "turbo", "FDD"
	};
	return names[led];
}

LedStatus::LedStatus(
		RTScheduler& rtScheduler,
		CommandController& commandController,
		MSXCliComm& msxCliComm_)
	: RTSchedulable(rtScheduler)
	, msxCliComm(msxCliComm_)
	, interp(commandController.getInterpreter())
{
	lastTime = Timer::getTime();
	for (int i = 0; i < NUM_LEDS; ++i) {
		ledValue[i] = false;
		std::string name = getLedName(static_cast<Led>(i));
		ledStatus[i] = std::make_unique<ReadOnlySetting>(
			commandController, "led_" + name,
			"Current status for LED: " + name,
			TclObject("off"));
	}
}

LedStatus::~LedStatus() = default;

void LedStatus::setLed(Led led, bool status)
{
	if (ledValue[led] == status) return;
	ledValue[led] = status;

	// Some MSX programs generate tons of LED events (e.g. New Era uses
	// the LEDs as a VU meter while playing samples). Without throttling
	// all these events overload the host CPU. That's why we limit it to
	// 100 events per second.
	auto now = Timer::getTime();
	auto diff = now - lastTime;
	if (diff > 10000) { // 1/100 s
		// handle now
		lastTime = now;
		handleEvent(led);
	} else {
		// schedule to handle it later, if we didn't plan to do so already
		if (!isPendingRT()) {
			scheduleRT(10000 - diff);
		}
	}
}

void LedStatus::handleEvent(Led led) noexcept
{
	std::string_view str = ledValue[led] ? "on": "off";
	ledStatus[led]->setReadOnlyValue(TclObject(str));
	msxCliComm.update(CliComm::LED, getLedName(led), str);
}

void LedStatus::executeRT()
{
	for (int i = 0; i < NUM_LEDS; ++i) {
		if (ledValue[i] != ledStatus[i]->getValue().getBoolean(interp)) {
			handleEvent(static_cast<Led>(i));
		}
	}
	lastTime = Timer::getTime();
}

} // namespace openmsx
