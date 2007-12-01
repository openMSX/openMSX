// $Id$

#include "EventTranslator.hh"
#include "EventDelay.hh"
#include "EventDistributor.hh"

namespace openmsx {

EventTranslator::EventTranslator(EventDistributor& eventDistributor_,
                                 EventDelay& eventDelay_)
	: eventDistributor(eventDistributor_)
	, eventDelay(eventDelay_)
{
	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT,   *this, EventDistributor::MSX);

	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_MOTION_EVENT,      *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT,   *this, EventDistributor::MSX);

	eventDistributor.registerEventListener(
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT,   *this, EventDistributor::MSX);
}

EventTranslator::~EventTranslator()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_UP_EVENT,   *this);

	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_MOTION_EVENT,      *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT,   *this);

	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT,   *this);
}

bool EventTranslator::signalEvent(shared_ptr<const Event> event)
{
	// TODO do some real translation here ;)
	//      (e.g. depending on host/MSX keyboard layout)
	eventDelay.queueEvent(event);
	return true;
}

} // namespace openmsx
