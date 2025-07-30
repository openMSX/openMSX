#include "LedStatus.hh"

#include "MSXCliComm.hh"
#include "CommandController.hh"
#include "Timer.hh"

#include "stl.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <string_view>

namespace openmsx {

using namespace std::literals;

[[nodiscard]] static std::string_view getLedName(LedStatus::Led led)
{
	static constexpr std::array<std::string_view, LedStatus::NUM_LEDS> names = {
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
	, ledStatus(generate_array<NUM_LEDS>([&](auto i) {
		return ReadOnlySetting(
			commandController,
			tmpStrCat("led_", getLedName(static_cast<Led>(i))),
			"Current status for LED",
			TclObject("off"));
	}))
	, lastTime(Timer::getTime())
{
	std::ranges::fill(ledValue, false);
}

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
	std::string_view str = ledValue[led] ? "on"sv : "off"sv;
	ledStatus[led].setReadOnlyValue(TclObject(str));
	msxCliComm.updateFiltered(CliComm::UpdateType::LED, getLedName(led), str);
}

void LedStatus::executeRT()
{
	for (auto i : xrange(int(NUM_LEDS))) {
		if (ledValue[i] != ledStatus[i].getValue().getBoolean(interp)) {
			handleEvent(static_cast<Led>(i));
		}
	}
	lastTime = Timer::getTime();
}

} // namespace openmsx
