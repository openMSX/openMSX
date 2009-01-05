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

void MSXEventDistributor::distributeEvent(EventPtr event, EmuTime::param time)
{
	// Iterate over a copy because signalEvent() may indirect call back into
	// registerEventListener().
	//   e.g. signalEvent() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerEventListener()
	Listeners copy = listeners;
	for (Listeners::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		(*it)->signalEvent(event, time);
	}
}

} // namespace openmsx
