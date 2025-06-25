#ifndef EVENTDELAY_HH
#define EVENTDELAY_HH

#include "EventListener.hh"
#include "Schedulable.hh"
#include "EmuTime.hh"
#include "Event.hh"
#include "FloatSetting.hh"

#include "build-info.hh"

#include <vector>
#include <deque>
#include <cstdint>

namespace openmsx {

class Scheduler;
class CommandController;
class EventDistributor;
class MSXEventDistributor;
class ReverseManager;

/** This class is responsible for translating host events into MSX events.
  * It also translates host event timing into EmuTime. To better do this
  * we introduce a small delay (default 0.03s) in this translation.
  */
class EventDelay final : private EventListener, private Schedulable
{
public:
	EventDelay(Scheduler& scheduler, CommandController& commandController,
	           EventDistributor& eventDistributor,
	           MSXEventDistributor& msxEventDistributor,
	           ReverseManager& reverseManager);
	~EventDelay();

	void sync(EmuTime curEmu);
	void flush();

private:
	// EventListener
	bool signalEvent(const Event& event) override;

	// Schedulable
	void executeUntil(EmuTime time) override;

private:
	EventDistributor& eventDistributor;
	MSXEventDistributor& msxEventDistributor;

	std::vector<Event> toBeScheduledEvents;
	std::deque<Event> scheduledEvents;

#if PLATFORM_ANDROID
	std::vector<std::pair<int, Event>> nonMatchedKeyPresses;
#endif

	EmuTime prevEmu = EmuTime::zero();
	uint64_t prevReal;
	FloatSetting delaySetting;
};

} // namespace openmsx

#endif
