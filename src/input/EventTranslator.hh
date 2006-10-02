// $Id$

#ifndef EVENTTRANSLATOR_HH
#define EVENTTRANSLATOR_HH

#include "EventListener.hh"
#include "noncopyable.hh"

namespace openmsx {

class EventDistributor;
class EventDelay;

class EventTranslator : private EventListener, private noncopyable
{
public:
	EventTranslator(EventDistributor& eventDistributor,
	                EventDelay& eventDelay);
	virtual ~EventTranslator();

private:
	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	EventDistributor& eventDistributor;
	EventDelay & eventDelay;
};

} // namespace openmsx

#endif
