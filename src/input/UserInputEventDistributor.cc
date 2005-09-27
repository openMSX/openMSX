// $Id$

#include "UserInputEventDistributor.hh"
#include "UserInputEventListener.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace openmsx {

UserInputEventDistributor::UserInputEventDistributor(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
	eventDistributor.registerEventListener(
		OPENMSX_EMU_KEY_DOWN_EVENT, *this, EventDistributor::EMU);
	eventDistributor.registerEventListener(
		OPENMSX_EMU_KEY_UP_EVENT, *this, EventDistributor::EMU);
	eventDistributor.registerEventListener(
		OPENMSX_CONSOLE_ON_EVENT, *this, EventDistributor::DETACHED);
	// TODO: Nobody listens to OPENMSX_CONSOLE_OFF_EVENT,
	//       so should we send it?
}

UserInputEventDistributor::~UserInputEventDistributor()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_EMU_KEY_DOWN_EVENT, *this, EventDistributor::EMU);
	eventDistributor.unregisterEventListener(
		OPENMSX_EMU_KEY_UP_EVENT, *this, EventDistributor::EMU);
	eventDistributor.unregisterEventListener(
		OPENMSX_CONSOLE_ON_EVENT, *this, EventDistributor::DETACHED);
	if (!listeners.empty()) {
		std::cerr << "~UserInputEventDistributor: "
		             "was not unregistered" << std::endl;
	}
}

void UserInputEventDistributor::registerEventListener(
	UserInputEventListener& listener)
{
	Listeners::iterator it =
		find(listeners.begin(), listeners.end(), &listener);
	assert(it == listeners.end());
	listeners.push_back(&listener);
}

void UserInputEventDistributor::unregisterEventListener(
	UserInputEventListener& listener)
{
	Listeners::iterator it =
		find(listeners.begin(), listeners.end(), &listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

void UserInputEventDistributor::signalEvent(const Event& event)
{
	assert(dynamic_cast<const UserInputEvent*>(&event));
	const UserInputEvent& userInputEvent =
		static_cast<const UserInputEvent&>(event);

	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->signalEvent(userInputEvent);
	}
}

} // namespace openmsx
