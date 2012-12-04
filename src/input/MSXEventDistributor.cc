// $Id$

#include "MSXEventDistributor.hh"
#include "MSXEventListener.hh"
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

void MSXEventDistributor::distributeEvent(const EventPtr& event, EmuTime::param time)
{
	// Iterate over a copy because signalEvent() may indirect call back into
	// registerEventListener().
	//   e.g. signalEvent() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerEventListener()
	auto copy = listeners;
	for (auto& l : copy) {
		if (isRegistered(l)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			l->signalEvent(event, time);
		}
	}
}

} // namespace openmsx
