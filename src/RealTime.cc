#include "RealTime.hh"
#include "Timer.hh"
#include "EventDistributor.hh"
#include "EventDelay.hh"
#include "Event.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "BooleanSetting.hh"
#include "ThrottleManager.hh"
#include "unreachable.hh"

namespace openmsx {

const double   SYNC_INTERVAL = 0.08;  // s
const int64_t  MAX_LAG       = 200000; // us
const uint64_t ALLOWED_LAG   =  20000; // us

RealTime::RealTime(
		MSXMotherBoard& motherBoard_, GlobalSettings& globalSettings,
		EventDelay& eventDelay_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, eventDistributor(motherBoard.getReactor().getEventDistributor())
	, eventDelay(eventDelay_)
	, speedManager   (globalSettings.getSpeedManager())
	, throttleManager(globalSettings.getThrottleManager())
	, pauseSetting   (globalSettings.getPauseSetting())
	, powerSetting   (globalSettings.getPowerSetting())
	, emuTime(EmuTime::zero())
	, enabled(true)
{
	speedManager.attach(*this);
	throttleManager.attach(*this);
	pauseSetting.attach(*this);
	powerSetting.attach(*this);

	resync();

	eventDistributor.registerEventListener(EventType::FINISH_FRAME, *this);
	eventDistributor.registerEventListener(EventType::FRAME_DRAWN,  *this);
}

RealTime::~RealTime()
{
	eventDistributor.unregisterEventListener(EventType::FRAME_DRAWN,  *this);
	eventDistributor.unregisterEventListener(EventType::FINISH_FRAME, *this);

	powerSetting.detach(*this);
	pauseSetting.detach(*this);
	throttleManager.detach(*this);
	speedManager.detach(*this);
}

double RealTime::getRealDuration(EmuTime::param time1, EmuTime::param time2)
{
	return (time2 - time1).toDouble() / speedManager.getSpeed();
}

EmuDuration RealTime::getEmuDuration(double realDur)
{
	return EmuDuration(realDur * speedManager.getSpeed());
}

bool RealTime::timeLeft(uint64_t us, EmuTime::param time)
{
	auto realDuration = static_cast<uint64_t>(
		getRealDuration(emuTime, time) * 1000000ULL);
	auto currentRealTime = Timer::getTime();
	return (currentRealTime + us) <
	           (idealRealTime + realDuration + ALLOWED_LAG);
}

void RealTime::sync(EmuTime::param time, bool allowSleep)
{
	if (allowSleep) {
		removeSyncPoint();
	}
	internalSync(time, allowSleep);
	if (allowSleep) {
		setSyncPoint(time + getEmuDuration(SYNC_INTERVAL));
	}
}

void RealTime::internalSync(EmuTime::param time, bool allowSleep)
{
	if (throttleManager.isThrottled()) {
		auto realDuration = static_cast<uint64_t>(
		        getRealDuration(emuTime, time) * 1000000ULL);
		idealRealTime += realDuration;
		auto currentRealTime = Timer::getTime();
		int64_t sleep = idealRealTime - currentRealTime;
		if (allowSleep) {
			// want to sleep for 'sleep' us
			sleep += static_cast<int64_t>(sleepAdjust);
			int64_t delta = 0;
			if (sleep > 0) {
				Timer::sleep(sleep); // request to sleep for 'sleep+sleepAdjust'
				int64_t slept = Timer::getTime() - currentRealTime;
				delta = sleep - slept; // actually slept for 'slept' us
			}
			const double ALPHA = 0.2;
			sleepAdjust = sleepAdjust * (1 - ALPHA) + delta * ALPHA;
		}
		if (-sleep > MAX_LAG) {
			idealRealTime = currentRealTime - MAX_LAG / 2;
		}
	}
	if (allowSleep) {
		eventDelay.sync(time);
	}

	emuTime = time;
}

void RealTime::executeUntil(EmuTime::param time)
{
	internalSync(time, true);
	setSyncPoint(time + getEmuDuration(SYNC_INTERVAL));
}

int RealTime::signalEvent(const Event& event) noexcept
{
	if (!motherBoard.isActive() || !enabled) {
		// these are global events, only the active machine should
		// synchronize with real time
		return 0;
	}
	visit(overloaded{
		[&](const FinishFrameEvent& ffe) {
			if (!ffe.needRender()) {
				// sync but don't sleep
				sync(getCurrentTime(), false);
			}
		},
		[&](const FrameDrawnEvent&) {
			// sync and possibly sleep
			sync(getCurrentTime(), true);
		},
		[&](const EventBase /*e*/) {
			UNREACHABLE;
		}
	}, event);
	return 0;
}

void RealTime::update(const Setting& /*setting*/) noexcept
{
	resync();
}

void RealTime::update(const SpeedManager& /*speedManager*/) noexcept
{
	resync();
}

void RealTime::update(const ThrottleManager& /*throttleManager*/) noexcept
{
	resync();
}

void RealTime::resync()
{
	if (!enabled) return;

	idealRealTime = Timer::getTime();
	sleepAdjust = 0.0;
	removeSyncPoint();
	emuTime = getCurrentTime();
	setSyncPoint(emuTime + getEmuDuration(SYNC_INTERVAL));
}

void RealTime::enable()
{
	enabled = true;
	resync();
}

void RealTime::disable()
{
	enabled = false;
	removeSyncPoint();
}

} // namespace openmsx
