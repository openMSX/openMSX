// $Id$

#include "UserInputEventDistributor.hh"
#include "UserInputEventListener.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include "FloatSetting.hh"
#include "Timer.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace openmsx {

UserInputEventDistributor::UserInputEventDistributor(
		Scheduler& scheduler,
		CommandController& commandController,
		EventDistributor& eventDistributor_)
	: Schedulable(scheduler)
	, eventDistributor(eventDistributor_)
	, console(false)
	, prevEmu(EmuTime::zero)
	, prevReal(Timer::getTime())
	, delaySetting(new FloatSetting(commandController, "inputdelay",
	               "EXPERIMENTAL: delay input to avoid keyskips",
	               0.03, 0.0, 10.0))
{
	eventDistributor.registerEventListener(
		OPENMSX_CONSOLE_ON_EVENT,  *this);
	eventDistributor.registerEventListener(
		OPENMSX_CONSOLE_OFF_EVENT, *this);

	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT,   *this);

	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_MOTION_EVENT,      *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT,   *this);

	eventDistributor.registerEventListener(
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT,   *this);
}

UserInputEventDistributor::~UserInputEventDistributor()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_CONSOLE_ON_EVENT,  *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_CONSOLE_OFF_EVENT, *this);

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

	if (!listeners.empty()) {
		std::cerr << "~UserInputEventDistributor: "
		             "was not unregistered" << std::endl;
	}
}

void UserInputEventDistributor::registerEventListener(
	UserInputEventListener& listener)
{
	assert(std::find(listeners.begin(), listeners.end(), &listener) ==
	       listeners.end());
	listeners.push_back(&listener);
}

void UserInputEventDistributor::unregisterEventListener(
	UserInputEventListener& listener)
{
	Listeners::iterator it =
		std::find(listeners.begin(), listeners.end(), &listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

void UserInputEventDistributor::signalEvent(const Event& event)
{
	EventType type = event.getType();
	if (type == OPENMSX_CONSOLE_ON_EVENT) {
		console = true;
	} else if (type == OPENMSX_CONSOLE_OFF_EVENT) {
		console = false;
	} else if ((type == OPENMSX_KEY_DOWN_EVENT) && !console) {
		const KeyEvent& keyEvent = static_cast<const KeyEvent&>(event);
		queueEvent(new KeyDownEvent(
			keyEvent.getKeyCode(), keyEvent.getUnicode()));
	} else if (type == OPENMSX_KEY_UP_EVENT) {
		const KeyEvent& keyEvent = static_cast<const KeyEvent&>(event);
		queueEvent(new KeyUpEvent(
			keyEvent.getKeyCode(), keyEvent.getUnicode()));
	} else if (type == OPENMSX_MOUSE_MOTION_EVENT) {
		const MouseMotionEvent& motionEvent =
			static_cast<const MouseMotionEvent&>(event);
		queueEvent(new MouseMotionEvent(
			motionEvent.getX(), motionEvent.getY()));
	} else if (type == OPENMSX_MOUSE_BUTTON_DOWN_EVENT) {
		const MouseButtonEvent& buttonEvent =
			static_cast<const MouseButtonEvent&>(event);
		queueEvent(new MouseButtonDownEvent(
			buttonEvent.getButton()));
	} else if (type == OPENMSX_MOUSE_BUTTON_UP_EVENT) {
		const MouseButtonEvent& buttonEvent =
			static_cast<const MouseButtonEvent&>(event);
		queueEvent(new MouseButtonUpEvent(
			buttonEvent.getButton()));
	} else if (type == OPENMSX_JOY_AXIS_MOTION_EVENT) {
		const JoystickAxisMotionEvent& motionEvent =
			static_cast<const JoystickAxisMotionEvent&>(event);
		queueEvent(new JoystickAxisMotionEvent(
			motionEvent.getJoystick(), motionEvent.getAxis(),
			motionEvent.getValue()));
	} else if (type == OPENMSX_JOY_BUTTON_DOWN_EVENT) {
		const JoystickButtonEvent& buttonEvent =
			static_cast<const JoystickButtonEvent&>(event);
		queueEvent(new JoystickButtonDownEvent(
			buttonEvent.getJoystick(), buttonEvent.getButton()));
	} else if (type == OPENMSX_JOY_BUTTON_UP_EVENT) {
		const JoystickButtonEvent& buttonEvent =
			static_cast<const JoystickButtonEvent&>(event);
		queueEvent(new JoystickButtonDownEvent(
			buttonEvent.getJoystick(), buttonEvent.getButton()));
	}
}

void UserInputEventDistributor::queueEvent(Event* event)
{
	toBeScheduledEvents.push_back(EventTime(event, Timer::getTime()));
}

void UserInputEventDistributor::sync(const EmuTime& emuTime)
{
	unsigned long long curRealTime = Timer::getTime();
	unsigned long long realDuration = curRealTime - prevReal;
	EmuDuration emuDuration = emuTime - prevEmu;

	double factor = emuDuration.toDouble() / realDuration;
	EmuDuration extraDelay(delaySetting->getValue());

	EmuTime time = prevEmu + extraDelay;
	for (std::vector<EventTime>::const_iterator it =
	        toBeScheduledEvents.begin();
	     it != toBeScheduledEvents.end(); ++it) {
		assert(it->time <= curRealTime);
		scheduledEvents.push_back(it->event);
		unsigned long long offset = curRealTime - it->time;
		EmuDuration emuOffset(factor * offset);
		EmuTime schedTime = time + emuOffset;
		if (schedTime < emuTime) {
			//PRT_DEBUG("input delay too short");
			schedTime = emuTime;
		}
		setSyncPoint(schedTime);
	}
	toBeScheduledEvents.clear();

	prevReal = curRealTime;
	prevEmu = emuTime;
}

void UserInputEventDistributor::executeUntil(
		const EmuTime& /*time*/, int /*userData*/)
{
	Event* event = scheduledEvents.front();
	scheduledEvents.pop_front();

	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->signalEvent(*event);
	}
	delete event;
}

const std::string& UserInputEventDistributor::schedName() const
{
	static const std::string name = "UserInputEventDistributor";
	return name;
}

} // namespace openmsx
