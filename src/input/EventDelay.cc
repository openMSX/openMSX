// $Id$

#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "MSXEventDistributor.hh"
#include "ReverseManager.hh"
#include "InputEvents.hh"
#include "FloatSetting.hh"
#include "Timer.hh"
#include "MSXException.hh"
#include "checked_cast.hh"
#include "memory.hh"
#include <cassert>

namespace openmsx {

EventDelay::EventDelay(Scheduler& scheduler,
                       CommandController& commandController,
                       EventDistributor& eventDistributor_,
                       MSXEventDistributor& msxEventDistributor_,
                       ReverseManager& reverseManager)
	: Schedulable(scheduler)
	, eventDistributor(eventDistributor_)
	, msxEventDistributor(msxEventDistributor_)
	, prevEmu(EmuTime::zero)
	, prevReal(Timer::getTime())
	, delaySetting(make_unique<FloatSetting>(
		commandController, "inputdelay",
		"delay input to avoid key-skips", 0.03, 0.0, 10.0))
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

	reverseManager.registerEventDelay(*this);
}

EventDelay::~EventDelay()
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

int EventDelay::signalEvent(const EventPtr& event)
{
	toBeScheduledEvents.push_back(event);
	return 0;
}

void EventDelay::sync(EmuTime::param curEmu)
{
	auto curRealTime = Timer::getTime();
	auto realDuration = curRealTime - prevReal;
	prevReal = curRealTime;
	auto emuDuration = curEmu - prevEmu;
	prevEmu = curEmu;

	double factor = emuDuration.toDouble() / realDuration;
	EmuDuration extraDelay(delaySetting->getValue());

	EmuTime time = curEmu + extraDelay;
	for (auto& e : toBeScheduledEvents) {
		scheduledEvents.push_back(e);
		auto timedEvent = checked_cast<const TimedEvent*>(e.get());
		auto eventRealTime = timedEvent->getRealTime();
		assert(eventRealTime <= curRealTime);
		auto offset = curRealTime - eventRealTime;
		EmuDuration emuOffset(factor * offset);
		auto schedTime = (emuOffset < extraDelay)
		               ? time - emuOffset
		               : curEmu;
		assert(curEmu <= schedTime);
		setSyncPoint(schedTime);
	}
	toBeScheduledEvents.clear();
}

void EventDelay::executeUntil(EmuTime::param time, int /*userData*/)
{
	try {
		EventPtr event = scheduledEvents.front(); // TODO move out
		scheduledEvents.pop_front();
		msxEventDistributor.distributeEvent(event, time);
	} catch (MSXException&) {
		// ignore
	}
}

void EventDelay::flush()
{
	EmuTime time = getCurrentTime();

	for (auto& e : scheduledEvents) {
		msxEventDistributor.distributeEvent(e, time);
	}
	scheduledEvents.clear();

	for (auto& e : toBeScheduledEvents) {
		msxEventDistributor.distributeEvent(e, time);
	}
	toBeScheduledEvents.clear();

	removeSyncPoints();
}

} // namespace openmsx
