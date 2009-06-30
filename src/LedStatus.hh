// $Id$

#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "EventListener.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class AlarmEvent;
class MSXMotherBoard;
class BooleanSetting;
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

	explicit LedStatus(MSXMotherBoard& motherBoard);
	~LedStatus();

	void setLed(Led led, bool status);

private:
	void handleEvent(Led led);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	MSXMotherBoard& motherBoard;
	const std::auto_ptr<AlarmEvent> alarm;
	std::auto_ptr<ReadOnlySetting<BooleanSetting> > ledStatus[NUM_LEDS];
	unsigned long long lastTime;
	bool ledValue[NUM_LEDS];
};

} // namespace openmsx

#endif
