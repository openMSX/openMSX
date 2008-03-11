// $Id$

#ifndef LEDSTATUS_HH
#define LEDSTATUS_HH

#include "LedEvent.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class BooleanSetting;
template <typename> class ReadOnlySetting;

class LedStatus : private noncopyable
{
public:
	explicit LedStatus(MSXMotherBoard& motherBoard);
	~LedStatus();

	void setLed(LedEvent::Led led, bool status);

private:
	MSXMotherBoard& motherBoard;
	std::auto_ptr<ReadOnlySetting<BooleanSetting> > ledStatus[LedEvent::NUM_LEDS];
};

} // namespace openmsx

#endif
