// $Id$

#include "UserInputEventDistributor.hh"
#include "UserInputEventListener.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace openmsx {

UserInputEventDistributor::UserInputEventDistributor()
{
	EventDistributor& eventDistributor(EventDistributor::instance());
	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::DETACHED );
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::DETACHED );
	eventDistributor.registerEventListener(
		OPENMSX_CONSOLE_ON_EVENT, *this, EventDistributor::DETACHED );
	// TODO: Nobody listens to OPENMSX_CONSOLE_OFF_EVENT,
	//       so should we send it?
}

UserInputEventDistributor::~UserInputEventDistributor()
{
	EventDistributor& eventDistributor(EventDistributor::instance());
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::DETACHED );
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::DETACHED );
	eventDistributor.unregisterEventListener(
		OPENMSX_CONSOLE_ON_EVENT, *this, EventDistributor::DETACHED );
	for (int i = 0; i < NUM_SLOTS; i++) {
		if (!listeners[i].empty()) {
			std::cerr << "~UserInputEventDistributor: "
				"slot " << i << " was not unregistered" << std::endl;
		}
	}
}

UserInputEventDistributor& UserInputEventDistributor::instance()
{
	static UserInputEventDistributor oneInstance;
	return oneInstance;
}

void UserInputEventDistributor::registerEventListener(
	ListenerSlot slot, UserInputEventListener& listener)
{
	Listeners::iterator it =
	   find(listeners[slot].begin(), listeners[slot].end(), &listener);
	assert(it == listeners[slot].end());
	listeners[slot].push_back(&listener);
}

void UserInputEventDistributor::unregisterEventListener(
	ListenerSlot slot, UserInputEventListener& listener)
{
	Listeners::iterator it =
	   find(listeners[slot].begin(), listeners[slot].end(), &listener);
	assert(it != listeners[slot].end());
	listeners[slot].erase(it);
}

void UserInputEventDistributor::signalEvent(const Event& event)
{
	assert(dynamic_cast<const UserInputEvent*>(&event));
	const UserInputEvent& userInputEvent =
		static_cast<const UserInputEvent&>(event);

	bool cont = true;
	for (int i = 0; cont && i < NUM_SLOTS; ++i) {
		for (Listeners::const_iterator it = listeners[i].begin();
		     it != listeners[i].end(); ++it) {
			cont &= (*it)->signalEvent(userInputEvent);
		}
	}
}

} // namespace openmsx
