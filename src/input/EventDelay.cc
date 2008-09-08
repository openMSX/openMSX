// $Id$

#include "EventDelay.hh"
#include "MSXEventDistributor.hh"
#include "FloatSetting.hh"
#include "Timer.hh"
#include "MSXException.hh"
#include <cassert>

namespace openmsx {

EventDelay::EventDelay(Scheduler& scheduler,
                       CommandController& commandController,
                       MSXEventDistributor& eventDistributor_)
	: Schedulable(scheduler)
	, eventDistributor(eventDistributor_)
	, prevEmu(EmuTime::zero)
	, prevReal(Timer::getTime())
	, delaySetting(new FloatSetting(commandController, "inputdelay",
	               "delay input to avoid key-skips",
	               0.03, 0.0, 10.0))
{
}

EventDelay::~EventDelay()
{
}

void EventDelay::queueEvent(EventPtr event)
{
	toBeScheduledEvents.push_back(EventTime(event, Timer::getTime()));
}

void EventDelay::sync(const EmuTime& emuTime)
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

void EventDelay::executeUntil(const EmuTime& time, int /*userData*/)
{
	try {
		eventDistributor.distributeEvent(scheduledEvents.front(), time);
	} catch (MSXException& e) {
		// ignore
	}
	scheduledEvents.pop_front();
}

const std::string& EventDelay::schedName() const
{
	static const std::string name = "EventDelay";
	return name;
}

} // namespace openmsx
