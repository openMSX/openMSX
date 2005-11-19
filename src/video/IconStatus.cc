// $Id$

#include "IconStatus.hh"
#include "EventDistributor.hh"
#include "Timer.hh"
#include "Display.hh"

namespace openmsx {

IconStatus::IconStatus(EventDistributor& eventDistributor_,
                     Display& display_)
	: eventDistributor(eventDistributor_)
	, display(display_)
{
	unsigned long long now = Timer::getTime();
	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		iconStatus[i] = false;
		iconTime[i] = now;
	}

	eventDistributor.registerEventListener(
		OPENMSX_LED_EVENT, *this, EventDistributor::DETACHED);
}

IconStatus::~IconStatus()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_LED_EVENT, *this, EventDistributor::DETACHED);
}

bool IconStatus::getStatus(int icon) const
{
	return iconStatus[icon];
}

unsigned long long IconStatus::getTime(int icon) const
{
	return iconTime[icon];
}

void IconStatus::signalEvent(const Event& event)
{
	assert(event.getType() == OPENMSX_LED_EVENT);
	const LedEvent& ledEvent = static_cast<const LedEvent&>(event);
	LedEvent::Led led = ledEvent.getLed();
	bool status = ledEvent.getStatus();
	if (status != iconStatus[led]) {
		iconStatus[led] = status;
		iconTime[led] = Timer::getTime();
		display.repaintDelayed(40000); // 25 fps
	}
}

} // namespace openmsx
