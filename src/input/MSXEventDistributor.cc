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

bool MSXEventDistributor::isRegistered(MSXEventListener* listener) const
{
	return find(listeners.begin(), listeners.end(), listener) !=
	       listeners.end();
}

void MSXEventDistributor::registerEventListener(MSXEventListener& listener)
{
	assert(!isRegistered(&listener));
	listeners.push_back(&listener);
}

void MSXEventDistributor::unregisterEventListener(MSXEventListener& listener)
{
	assert(isRegistered(&listener));
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
		if (isRegistered(*it)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			(*it)->signalEvent(event, time);
		}
	}
}

} // namespace openmsx
