#include "RealTime.hh"

#include "BooleanSetting.hh"
#include "Event.hh"
#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "ThrottleManager.hh"
#include "Timer.hh"

#include "narrow.hh"
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
{
	speedManager.attach(*this);
	throttleManager.attach(*this);
	pauseSetting.attach(*this);
	powerSetting.attach(*this);

	resync();

	for (auto type : {EventType::FINISH_FRAME, EventType::FRAME_DRAWN}) {
		eventDistributor.registerEventListener(type, *this);
	}
}

RealTime::~RealTime()
{
	for (auto type : {EventType::FRAME_DRAWN, EventType::FINISH_FRAME}) {
		eventDistributor.unregisterEventListener(type, *this);
	}
	powerSetting.detach(*this);
	pauseSetting.detach(*this);
	throttleManager.detach(*this);
	speedManager.detach(*this);
}

double RealTime::getRealDuration(EmuTime time1, EmuTime time2) const
{
	return (time2 - time1).toDouble() / speedManager.getSpeed();
}

EmuDuration RealTime::getEmuDuration(double realDur) const
{
	return EmuDuration::sec(realDur * speedManager.getSpeed());
}

bool RealTime::timeLeft(uint64_t us, EmuTime time) const
{
	auto realDuration = static_cast<uint64_t>(
		getRealDuration(emuTime, time) * 1000000ULL);
	auto currentRealTime = Timer::getTime();
	return (currentRealTime + us) <
	           (idealRealTime + realDuration + ALLOWED_LAG);
}

void RealTime::sync(EmuTime time, bool allowSleep)
{
	if (allowSleep) {
		removeSyncPoint();
	}
	internalSync(time, allowSleep);
	if (allowSleep) {
		setSyncPoint(time + getEmuDuration(SYNC_INTERVAL));
	}
}

void RealTime::internalSync(EmuTime time, bool allowSleep)
{
	if (throttleManager.isThrottled()) {
		auto realDuration = static_cast<uint64_t>(
		        getRealDuration(emuTime, time) * 1000000ULL);
		idealRealTime += realDuration;
		auto currentRealTime = Timer::getTime();
		auto sleep = narrow_cast<int64_t>(idealRealTime - currentRealTime);
		if (allowSleep) {
			// want to sleep for 'sleep' us
			sleep += narrow_cast<int64_t>(sleepAdjust);
			int64_t delta = 0;
			if (sleep > 0) {
				Timer::sleep(sleep); // request to sleep for 'sleep+sleepAdjust'
				auto slept = narrow<int64_t>(Timer::getTime() - currentRealTime);
				delta = sleep - slept; // actually slept for 'slept' us
			}
			const double ALPHA = 0.2;
			sleepAdjust = sleepAdjust * (1 - ALPHA) + narrow_cast<double>(delta) * ALPHA;
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

void RealTime::executeUntil(EmuTime time)
{
	internalSync(time, true);
	setSyncPoint(time + getEmuDuration(SYNC_INTERVAL));
}

bool RealTime::signalEvent(const Event& event)
{
	if (!motherBoard.isActive() || !enabled) {
		// these are global events, only the active machine should
		// synchronize with real time
		return false;
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
	return false;
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
