// $Id$

#include "MSXEventDistributor.hh"
#include "MSXEventListener.hh"
#include "Event.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

MSXEventDistributor::MSXEventDistributor()
{
}

MSXEventDistributor::~MSXEventDistributor()
{
	assert(listeners.empty());
}

void MSXEventDistributor::registerEventListener(MSXEventListener& listener)
{
	assert(find(listeners.begin(), listeners.end(), &listener) ==
	       listeners.end());
	listeners.push_back(&listener);
}

void MSXEventDistributor::unregisterEventListener(MSXEventListener& listener)
{
	assert(count(listeners.begin(), listeners.end(), &listener) == 1);
	listeners.erase(find(listeners.begin(), listeners.end(), &listener));
}

void MSXEventDistributor::distributeEvent(EventPtr event)
{
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->signalEvent(event);
	}
}

} // namespace openmsx
