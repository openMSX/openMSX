#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "EventListener.hh"
#include "noncopyable.hh"
#include <memory>
#include <cstdint>

namespace openmsx {

class AlarmEvent;
class CommandController;
class EventDistributor;
class MSXCliComm;
class ReadOnlySetting;
class Interpreter;

class LedStatus : public EventListener, private noncopyable
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

	explicit LedStatus(
		EventDistributor& eventDistributor,
		CommandController& commandController,
		MSXCliComm& msxCliComm);
	~LedStatus();

	void setLed(Led led, bool status);

private:
	void handleEvent(Led led);

	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	MSXCliComm& msxCliComm;
	Interpreter& interp;
	const std::unique_ptr<AlarmEvent> alarm;
	std::unique_ptr<ReadOnlySetting> ledStatus[NUM_LEDS];
	uint64_t lastTime;
	bool ledValue[NUM_LEDS];
};

} // namespace openmsx

#endif
