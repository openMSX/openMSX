#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "MSXEventDistributor.hh"
#include "ReverseManager.hh"
#include "Event.hh"
#include "Timer.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include <cassert>

namespace openmsx {

EventDelay::EventDelay(Scheduler& scheduler_,
                       CommandController& commandController,
                       EventDistributor& eventDistributor_,
                       MSXEventDistributor& msxEventDistributor_,
                       ReverseManager& reverseManager)
	: Schedulable(scheduler_)
	, eventDistributor(eventDistributor_)
	, msxEventDistributor(msxEventDistributor_)
	, prevEmu(EmuTime::zero())
	, prevReal(Timer::getTime())
	, delaySetting(
		commandController, "inputdelay",
		"delay input to avoid key-skips", 0.0, 0.0, 10.0)
{
	eventDistributor.registerEventListener(
		EventType::KEY_DOWN, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::KEY_UP,   *this, EventDistributor::MSX);

	eventDistributor.registerEventListener(
		EventType::MOUSE_MOTION,      *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_DOWN, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_UP,   *this, EventDistributor::MSX);

	eventDistributor.registerEventListener(
		EventType::JOY_AXIS_MOTION, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::JOY_HAT,         *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_DOWN, *this, EventDistributor::MSX);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_UP,   *this, EventDistributor::MSX);

	reverseManager.registerEventDelay(*this);
}

EventDelay::~EventDelay()
{
	eventDistributor.unregisterEventListener(
		EventType::KEY_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::KEY_UP,   *this);

	eventDistributor.unregisterEventListener(
		EventType::MOUSE_MOTION,      *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_BUTTON_UP,   *this);

	eventDistributor.unregisterEventListener(
		EventType::JOY_AXIS_MOTION, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_HAT,         *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_BUTTON_UP,   *this);
}

int EventDelay::signalEvent(const Event& event) noexcept
{
	toBeScheduledEvents.push_back(event);
	if (delaySetting.getDouble() == 0.0) {
		sync(getCurrentTime());
	}
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
	EmuDuration extraDelay(delaySetting.getDouble());

#if PLATFORM_ANDROID
	// The virtual keyboard on Android sends a key press and the
	// corresponding key release event directly after each other, without a
	// delay. It sends both events either when the user has finished a
	// short tap or alternatively after the user has hold the button
	// pressed for a few seconds and then has selected the appropriate
	// character from the multi-character-popup that the virtual keyboard
	// displays when the user holds a button pressed for a short while.
	// Either way, the key release event comes way too short after the
	// press event for the MSX to process it. The two events follow each
	// other within a few milliseconds at most. Therefore, on Android,
	// special logic must be foreseen to delay the release event for a
	// short while. This special logic correlates each key release event
	// with the corresponding press event for the same key. If they are
	// less then 2/50 second apart, the release event gets delayed until
	// the next sync call. The 2/50 second has been chosen because it can
	// take up to 2 vertical interrupts (2 screen refreshes) for the MSX to
	// see the key press in the keyboard matrix, thus, 2/50 seconds is the
	// minimum delay required for an MSX running in PAL mode.
	std::vector<Event> toBeRescheduledEvents;
#endif

	EmuTime time = curEmu + extraDelay;
	for (auto& e : toBeScheduledEvents) {
#if PLATFORM_ANDROID
		if (getType(e) == one_of(EventType::KEY_DOWN, EventType::KEY_UP)) {
			const auto& keyEvent = get<KeyEvent>(e);
			int maskedKeyCode = int(keyEvent.getKeyCode()) & int(Keys::K_MASK);
			auto it = ranges::find(nonMatchedKeyPresses, maskedKeyCode,
			                       [](const auto& p) { return p.first; });
			if (getType(e) == EventType::KEY_DOWN) {
				if (it == end(nonMatchedKeyPresses)) {
					nonMatchedKeyPresses.emplace_back(maskedKeyCode, e);
				} else {
					it->second = e;
				}
			} else {
				if (it != end(nonMatchedKeyPresses)) {
					const auto& timedPressEvent   = get<TimedEvent>(it->second);
					const auto& timedReleaseEvent = get<TimedEvent>(e);
					auto pressRealTime = timedPressEvent.getRealTime();
					auto releaseRealTime = timedReleaseEvent.getRealTime();
					auto deltaTime = releaseRealTime - pressRealTime;
					if (deltaTime <= 2000000 / 50) {
						// The key release came less then 2 MSX interrupts from the key press.
						// Reschedule it for the next sync, with the realTime updated to now, so that it seems like the
						// key was released now and not when android released it.
						// Otherwise, the offset calculation for the emutime further down below will go wrong on the next sync
						Event newKeyupEvent = Event::create<KeyUpEvent>(keyEvent.getKeyCode());
						toBeRescheduledEvents.push_back(newKeyupEvent);
						continue; // continue with next to be scheduled event
					}
					move_pop_back(nonMatchedKeyPresses, it);
				}
			}
		}
#endif
		scheduledEvents.push_back(e);
		const auto& timedEvent = get<TimedEvent>(e);
		auto eventRealTime = timedEvent.getRealTime();
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

#if PLATFORM_ANDROID
	append(toBeScheduledEvents, std::move(toBeRescheduledEvents));
#endif
}

void EventDelay::executeUntil(EmuTime::param time)
{
	try {
		auto event = std::move(scheduledEvents.front());
		scheduledEvents.pop_front();
		msxEventDistributor.distributeEvent(std::move(event), time);
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
