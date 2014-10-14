#include "MSXEventDistributor.hh"
#include "MSXEventListener.hh"
#include "stl.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

MSXEventDistributor::~MSXEventDistributor()
{
	assert(listeners.empty());
}

bool MSXEventDistributor::isRegistered(MSXEventListener* listener) const
{
	return contains(listeners, listener);
}

void MSXEventDistributor::registerEventListener(MSXEventListener& listener)
{
	assert(!isRegistered(&listener));
	listeners.push_back(&listener);
}

void MSXEventDistributor::unregisterEventListener(MSXEventListener& listener)
{
	move_pop_back(listeners, rfind_unguarded(listeners, &listener));
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
