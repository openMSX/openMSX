// $Id$

#include "AlarmEvent.hh"

namespace openmsx {

AlarmEvent::AlarmEvent(EventDistributor& distributor_,
                       EventListener& listener_,
                       EventType type_,
                       EventDistributor::Priority priority)
	: distributor(distributor_)
	, listener(listener_)
	, type(type_)
{
	distributor.registerEventListener(type, listener, priority);
}

AlarmEvent::~AlarmEvent()
{
	prepareDelete();
	distributor.unregisterEventListener(type, listener);
}

bool AlarmEvent::alarm()
{
	// Runs in timer thread.
	// Schedule event so that the main thread can do the real work.
	distributor.distributeEvent(new SimpleEvent(type));
	return false; // don't repeat
}

} // namespace openmsx
