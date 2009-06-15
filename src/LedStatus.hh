// $Id$

#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "Alarm.hh"
#include "EventListener.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class BooleanSetting;
template <typename> class ReadOnlySetting;

class LedStatus : private Alarm, private EventListener
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

	// Alarm
	virtual bool alarm();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	MSXMotherBoard& motherBoard;
	std::auto_ptr<ReadOnlySetting<BooleanSetting> > ledStatus[NUM_LEDS];
	unsigned long long lastTime;
	bool ledValue[NUM_LEDS];
};

} // namespace openmsx

#endif
