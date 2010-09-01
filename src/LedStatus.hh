// $Id$

#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "EventListener.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class AlarmEvent;
class BooleanSetting;
class CommandController;
class EventDistributor;
class MSXCliComm;
template <typename> class ReadOnlySetting;

class LedStatus : private EventListener, private noncopyable
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
	virtual int signalEvent(shared_ptr<const Event> event);

	MSXCliComm& msxCliComm;
	const std::auto_ptr<AlarmEvent> alarm;
	std::auto_ptr<ReadOnlySetting<BooleanSetting> > ledStatus[NUM_LEDS];
	unsigned long long lastTime;
	bool ledValue[NUM_LEDS];
};

} // namespace openmsx

#endif
