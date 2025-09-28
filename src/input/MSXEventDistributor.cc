#include "MSXEventDistributor.hh"

#include "MSXEventListener.hh"

#include "stl.hh"

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

void MSXEventDistributor::distributeEvent(const Event& event, EmuTime time)
{
	// Iterate over a copy because signalMSXEvent() may indirect call back into
	// registerEventListener().
	//   e.g. signalMSXEvent() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerEventListener()
	// 'listenersCopy' could be a local variable. But making it a member
	// variable allows to reuse the allocated vector-capacity.
	listenersCopy = listeners;
	for (auto& l : listenersCopy) {
		if (isRegistered(l)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			l->signalMSXEvent(event, time);
		}
	}
}

} // namespace openmsx
