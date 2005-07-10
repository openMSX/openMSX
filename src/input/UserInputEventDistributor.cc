// $Id$

#include "UserInputEventDistributor.hh"
#include "UserInputEventListener.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include <cassert>
#include <iostream>


namespace openmsx {

UserInputEventDistributor::UserInputEventDistributor()
{
	for (int i = 0; i < NUM_SLOTS; i++) {
		listeners[i] = NULL;
	}
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
		if (listeners[i] != NULL) {
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
	ListenerSlot slot, UserInputEventListener& listener )
{
	assert(listeners[slot] == NULL);
	listeners[slot] = &listener;
}

void UserInputEventDistributor::unregisterEventListener(
	ListenerSlot slot, UserInputEventListener& listener )
{
	assert(listeners[slot] == &listener);
	listeners[slot] = NULL;
}

void UserInputEventDistributor::signalEvent(const Event& event)
{
	assert(dynamic_cast<const UserInputEvent*>(&event));
	const UserInputEvent& userInputEvent =
		static_cast<const UserInputEvent&>(event);

	bool cont = true;
	for (int i = 0; cont && i < NUM_SLOTS; i++) {
		UserInputEventListener* listener = listeners[i];
		if (listener) {
			cont = listener->signalEvent(userInputEvent);
		}
	}
}

} // namespace openmsx
