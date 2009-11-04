// $Id$

#include "StateChangeDistributor.hh"
#include "StateChangeListener.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

StateChangeDistributor::StateChangeDistributor()
	: replaying(true)
{
}

StateChangeDistributor::~StateChangeDistributor()
{
	assert(listeners.empty());
}

bool StateChangeDistributor::isRegistered(StateChangeListener* listener) const
{
	return find(listeners.begin(), listeners.end(), listener) !=
	       listeners.end();
}

void StateChangeDistributor::registerListener(StateChangeListener& listener)
{
	assert(!isRegistered(&listener));
	listeners.push_back(&listener);
}

void StateChangeDistributor::unregisterListener(StateChangeListener& listener)
{
	assert(isRegistered(&listener));
	listeners.erase(find(listeners.begin(), listeners.end(), &listener));
}

void StateChangeDistributor::distributeNew(EventPtr event)
{
	if (replaying) {
		replaying = false;
		for (Listeners::const_iterator it = listeners.begin();
		     it != listeners.end(); ++it) {
			(*it)->stopReplay();
		}
	}
	distribute(event);
}

void StateChangeDistributor::distributeReplay(EventPtr event)
{
	assert(replaying);
	distribute(event);
}

void StateChangeDistributor::distribute(EventPtr event)
{
	// Iterate over a copy because signalStateChange() may indirect call
	// back into registerListener().
	//   e.g. signalStateChange() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerListener()
	Listeners copy = listeners;
	for (Listeners::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		if (isRegistered(*it)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			(*it)->signalStateChange(event);
		}
	}
}

} // namespace openmsx
