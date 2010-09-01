// $Id$

#ifndef EVENTDELAY_HH
#define EVENTDELAY_HH

#include "EventListener.hh"
#include "Schedulable.hh"
#include "EmuTime.hh"
#include <vector>
#include <deque>
#include <memory>

namespace openmsx {

class Scheduler;
class CommandController;
class Event;
class EventDistributor;
class MSXEventDistributor;
class ReverseManager;
class FloatSetting;

/** This class is responsible for translating host events into MSX events.
  * It also translates host event timing into EmuTime. To better do this
  * we introduce a small delay (default 0.03s) in this translation.
  */
class EventDelay : private EventListener, private Schedulable
{
public:
	EventDelay(Scheduler& scheduler, CommandController& commandController,
	           EventDistributor& eventDistributor,
	           MSXEventDistributor& msxEventDistributor,
	           ReverseManager& reverseManager);
	virtual ~EventDelay();

	void sync(EmuTime::param time);
	void flush();

private:
	typedef shared_ptr<const Event> EventPtr;

	// EventListener
	virtual int signalEvent(EventPtr event);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	EventDistributor& eventDistributor;
	MSXEventDistributor& msxEventDistributor;

	std::vector<EventPtr> toBeScheduledEvents;
	std::deque<EventPtr> scheduledEvents;

	EmuTime prevEmu;
	unsigned long long prevReal;
	const std::auto_ptr<FloatSetting> delaySetting;
};

} // namespace openmsx

#endif
