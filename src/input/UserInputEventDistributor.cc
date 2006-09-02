// $Id$

#include "UserInputEventDistributor.hh"
#include "UserInputEventListener.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "Timer.hh"
#include "checked_cast.hh"
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
	, prevEmu(EmuTime::zero)
	, prevReal(Timer::getTime())
	, delaySetting(new FloatSetting(commandController, "inputdelay",
	               "EXPERIMENTAL: delay input to avoid keyskips",
	               0.03, 0.0, 10.0))
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

UserInputEventDistributor::~UserInputEventDistributor()
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

bool UserInputEventDistributor::signalEvent(EventPtr event)
{
	queueEvent(event);
	return true;
}

void UserInputEventDistributor::queueEvent(EventPtr event)
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
	EventPtr event = scheduledEvents.front();
	scheduledEvents.pop_front();

	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->signalEvent(event);
	}
}

const std::string& UserInputEventDistributor::schedName() const
{
	static const std::string name = "UserInputEventDistributor";
	return name;
}

} // namespace openmsx
