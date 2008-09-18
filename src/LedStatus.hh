// $Id$

#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class BooleanSetting;
template <typename> class ReadOnlySetting;

class LedStatus : private noncopyable
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
	MSXMotherBoard& motherBoard;
	std::auto_ptr<ReadOnlySetting<BooleanSetting> > ledStatus[NUM_LEDS];
};

} // namespace openmsx

#endif
