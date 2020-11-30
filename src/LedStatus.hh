#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "RTSchedulable.hh"
#include <memory>
#include <cstdint>

namespace openmsx {

class CommandController;
class MSXCliComm;
class ReadOnlySetting;
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
	~LedStatus();

	void setLed(Led led, bool status);

private:
	void handleEvent(Led led) noexcept;

	// RTSchedulable
	void executeRT() override;

private:
	MSXCliComm& msxCliComm;
	Interpreter& interp;
	std::unique_ptr<ReadOnlySetting> ledStatus[NUM_LEDS];
	uint64_t lastTime;
	bool ledValue[NUM_LEDS];
};

} // namespace openmsx

#endif
