// $Id$

#include <cassert>
#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "Timer.hh"
#include "EventDistributor.hh"

namespace openmsx {

const float    SYNC_INTERVAL = 0.125;  // s
const int      MAX_LAG       = 200000; // us
const unsigned ALLOWED_LAG   =  20000; // us

RealTime::RealTime()
	: scheduler(Scheduler::instance()),
	  settingsConfig(SettingsConfig::instance()),
	  speedSetting("speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000),
	  throttleSetting("throttle", "controls speed throttling", true),
	  pauseSetting(scheduler.getPauseSetting()),
	  powerSetting(scheduler.getPowerSetting())
{
	speedSetting.addListener(this);
	throttleSetting.addListener(this);
	pauseSetting.addListener(this);
	powerSetting.addListener(this);
	
	scheduler.setSyncPoint(Scheduler::ASAP, this);

	resync();
}

RealTime::~RealTime()
{
	scheduler.removeSyncPoint(this);
	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
	throttleSetting.removeListener(this);
	speedSetting.removeListener(this);
}

RealTime& RealTime::instance()
{
	static RealTime oneInstance;
	return oneInstance;
}

float RealTime::getRealDuration(const EmuTime& time1, const EmuTime& time2)
{
	return (time2 - time1).toFloat() * 100.0 / speedSetting.getValue();
}

EmuDuration RealTime::getEmuDuration(float realDur)
{
	return EmuDuration(realDur * speedSetting.getValue() / 100.0);
}

bool RealTime::timeLeft(unsigned us, const EmuTime& time)
{
	unsigned realDuration = (unsigned)(getRealDuration(emuTime, time) * 1000000);
	unsigned currentRealTime = Timer::getTime();
	return (currentRealTime + us) < (idealRealTime + realDuration + ALLOWED_LAG);
}

void RealTime::sync(const EmuTime& time, bool allowSleep)
{
	scheduler.removeSyncPoint(this);
	internalSync(time, allowSleep);
}

void RealTime::internalSync(const EmuTime& time, bool allowSleep)
{
	if (throttleSetting.getValue()) {
		unsigned realDuration = (unsigned)(getRealDuration(emuTime, time) * 1000000);
		idealRealTime += realDuration;
		unsigned currentRealTime = Timer::getTime();
		int sleep = idealRealTime - currentRealTime;
		if (allowSleep) {
			PRT_DEBUG("RT: want to sleep " << sleep << "us");
			sleep += (int)sleepAdjust;
			int delta = 0;
			if (sleep > 0) {
				PRT_DEBUG("RT: Sleeping for " << sleep << "us");
				Timer::sleep(sleep);
				int slept = Timer::getTime() - currentRealTime;
				PRT_DEBUG("RT: Realy slept for " << slept << "us");
				delta = sleep - slept;
			}
			const float ALPHA = 0.2;
			sleepAdjust = sleepAdjust * (1 - ALPHA) + delta * ALPHA;
			PRT_DEBUG("RT: SleepAdjust: " << sleepAdjust);
		}
		if (-sleep > MAX_LAG) {
			idealRealTime = currentRealTime - MAX_LAG / 2;
		}
	}
	if (allowSleep) {
		EventDistributor::instance().sync(time);
	}

	emuTime = time;
	
	// Schedule again in future
	scheduler.setSyncPoint(time + getEmuDuration(SYNC_INTERVAL), this);
}

void RealTime::executeUntil(const EmuTime& time, int userData) throw()
{
	internalSync(time, true);
}

const string &RealTime::schedName() const
{
	static const string name("RealTime");
	return name;
}

void RealTime::update(const SettingLeafNode* setting) throw()
{
	resync();
}

void RealTime::resync()
{
	idealRealTime = Timer::getTime();
	sleepAdjust = 0.0;
	scheduler.removeSyncPoint(this);
	scheduler.setSyncPoint(Scheduler::ASAP, this);
}

} // namespace openmsx
