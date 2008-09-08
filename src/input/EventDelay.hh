// $Id$

#ifndef EVENTDELAY_HH
#define EVENTDELAY_HH

#include "Schedulable.hh"
#include "Event.hh"
#include "EmuTime.hh"
#include "shared_ptr.hh"
#include <vector>
#include <deque>
#include <memory>

namespace openmsx {

class Scheduler;
class CommandController;
class MSXEventDistributor;
class FloatSetting;

class EventDelay : private Schedulable
{
public:
	typedef shared_ptr<const Event> EventPtr;

	EventDelay(Scheduler& scheduler, CommandController& commandController,
	           MSXEventDistributor& eventDistributor);
	virtual ~EventDelay();

	void queueEvent(EventPtr event);
	void sync(const EmuTime& time);

private:
	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	MSXEventDistributor& eventDistributor;

	struct EventTime {
		EventTime(EventPtr event_, unsigned long long time_)
			: event(event_), time(time_) {}
		EventPtr event;
		unsigned long long time;
	};
	std::vector<EventTime> toBeScheduledEvents;
	std::deque<EventPtr> scheduledEvents;

	EmuTime prevEmu;
	unsigned long long prevReal;
	const std::auto_ptr<FloatSetting> delaySetting;
};

} // namespace openmsx

#endif
