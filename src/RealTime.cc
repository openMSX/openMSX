// $Id$

#include <cassert>
#include <memory> // for auto_ptr
#include "config.h"
#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "RealTimeSDL.hh"
#include "RealTimeRTC.hh"

using std::auto_ptr;

namespace openmsx {

const int SYNC_INTERVAL = 500000; // us
const int MAX_LAG       = 200000; // us
const int ALLOWED_LAG   =  20000; // us


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
}

void RealTime::initBase()
{
	reset();
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
	static auto_ptr<RealTime> oneInstance;
	if (!oneInstance.get()) {
#ifdef HAVE_LINUX_RTC_H
		oneInstance.reset(RealTimeRTC::create());
#endif
		if (!oneInstance.get()) {
			oneInstance.reset(new RealTimeSDL());
		}
	}
	return *oneInstance.get();
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
	unsigned currentRealTime = getRealTime();
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
		unsigned currentRealTime = getRealTime();
		int sleep = idealRealTime - currentRealTime;
		if (allowSleep && (sleep > 0)) {
			doSleep(sleep);
		}
		if (-sleep > MAX_LAG) {
			idealRealTime = currentRealTime - MAX_LAG / 2;
		}
	}
	emuTime = time;
	
	// Schedule again in future
	EmuTimeFreq<1000000> time2(time);
	scheduler.setSyncPoint(time2 + SYNC_INTERVAL, this);
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
	idealRealTime = getRealTime();
}

} // namespace openmsx
