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

using std::make_shared;

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
		"delay input to avoid key-skips", 0.0, 0.0, 10.0))
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
	if (delaySetting->getValue() == 0.0) {
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
	EmuDuration extraDelay(delaySetting->getValue());

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
	std::vector<EventPtr> toBeRescheduledEvents;
#endif

	EmuTime time = curEmu + extraDelay;
	for (auto& e : toBeScheduledEvents) {
#if PLATFORM_ANDROID
		if (e->getType() == OPENMSX_KEY_DOWN_EVENT ||
		    e->getType() == OPENMSX_KEY_UP_EVENT) {
			auto keyEvent = checked_cast<const KeyEvent*>(e.get());
			int maskedKeyCode = int(keyEvent->getKeyCode()) & int(Keys::K_MASK);
			if (e->getType() == OPENMSX_KEY_DOWN_EVENT) {
				nonMatchedKeyPresses[maskedKeyCode] = e;
			} else {
				auto nonMatchedKeyPressesIterator = nonMatchedKeyPresses.find(maskedKeyCode);
				if (nonMatchedKeyPressesIterator != nonMatchedKeyPresses.end()) {
					auto timedPressEvent = checked_cast<const TimedEvent*>(nonMatchedKeyPressesIterator->second.get());
					auto timedReleaseEvent = checked_cast<const TimedEvent*>(e.get());
					auto pressRealTime = timedPressEvent->getRealTime();
					auto releaseRealTime = timedReleaseEvent->getRealTime();
					auto deltaTime = releaseRealTime - pressRealTime;
					if (deltaTime <= 2000000 / 50) {
						// The key release came less then 2 MSX interrupts from the key press.
						// Reschedule it for the next sync, with the realTime updated to now, so that it seems like the
						// key was released now and not when android released it.
						// Otherwise, the offset calculation for the emutime further down below will go wrong on the next sync
						EventPtr newKeyupEvent = make_shared<KeyUpEvent>(keyEvent->getKeyCode(), keyEvent->getUnicode());
						toBeRescheduledEvents.push_back(newKeyupEvent);
						continue; // continue with next to be scheduled event
					}
					nonMatchedKeyPresses.erase(nonMatchedKeyPressesIterator);
				}
			}
		}
#endif
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

#if PLATFORM_ANDROID
	toBeScheduledEvents.insert(toBeScheduledEvents.end(),
		make_move_iterator(toBeRescheduledEvents.begin()),
		make_move_iterator(toBeRescheduledEvents.end()));
#endif
}

void EventDelay::executeUntil(EmuTime::param time, int /*userData*/)
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
