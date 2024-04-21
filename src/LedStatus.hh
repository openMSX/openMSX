#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "ReadOnlySetting.hh"
#include "RTSchedulable.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class CommandController;
class MSXCliComm;
class RTScheduler;
class Interpreter;

class LedStatus final : public RTSchedulable
{
public:
	enum Led {
		POWER,
		CAPS,
		KANA, // same as CODE LED
		PAUSE,
		TURBO,
		FDD,
		NUM_LEDS // must be last
	};

	LedStatus(RTScheduler& rtScheduler,
	          CommandController& commandController,
	          MSXCliComm& msxCliComm);

	void setLed(Led led, bool status);

private:
	void handleEvent(Led led) noexcept;

	// RTSchedulable
	void executeRT() override;

private:
	MSXCliComm& msxCliComm;
	Interpreter& interp;
	std::array<ReadOnlySetting, NUM_LEDS> ledStatus;
	uint64_t lastTime;
	std::array<bool, NUM_LEDS> ledValue;
};

} // namespace openmsx

#endif
