// $Id$

#include <cassert>
#include <memory> // for auto_ptr
#include "config.h"
#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "RealTimeSDL.hh"
#include "RealTimeRTC.hh"

using std::auto_ptr;

namespace openmsx {

const int SYNC_INTERVAL = 50;


RealTime::RealTime()
	: scheduler(Scheduler::instance()),
	  msxConfig(MSXConfig::instance()),
	  speedSetting("speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000),
	  throttleSetting("throttle", "controls speed throttling", true),
	  pauseSetting(scheduler.getPauseSetting()),
	  powerSetting(scheduler.getPowerSetting())
{
	speedSetting.addListener(this);
	pauseSetting.addListener(this);
	powerSetting.addListener(this);
	
	maxCatchUpTime = 2000;	// ms
	maxCatchUpFactor = 105; // %
	try {
		Config *config = msxConfig.getConfigById("RealTime");
		maxCatchUpTime = config->getParameterAsInt("max_catch_up_time", maxCatchUpTime);
		maxCatchUpFactor = config->getParameterAsInt("max_catch_up_factor", maxCatchUpFactor);
	} catch (ConfigException &e) {
		// no Realtime section
	}
	
	scheduler.setSyncPoint(Scheduler::ASAP, this);
}

RealTime::~RealTime()
{
	scheduler.removeSyncPoint(this);
	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
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

const string &RealTime::schedName() const
{
	static const string name("RealTime");
	return name;
}

void RealTime::executeUntil(const EmuTime& curEmu, int userData) throw()
{
	internalSync(curEmu);
}

float RealTime::sync(const EmuTime &time)
{
	scheduler.removeSyncPoint(this);
	return internalSync(time);
}

float RealTime::internalSync(const EmuTime &time)
{
	float speed;
	if (throttleSetting.getValue()) {
		speed = doSync(time);
	} else {
		speed = 1.0;
		resync();
	}
	
	// Schedule again in future
	EmuTimeFreq<1000> time2(time);
	scheduler.setSyncPoint(time2 + SYNC_INTERVAL, this);
	
	return speed;
}

float RealTime::getRealDuration(const EmuTime &time1, const EmuTime &time2)
{
	return (time2 - time1).toFloat() * 100.0 / speedSetting.getValue();
}

EmuDuration RealTime::getEmuDuration(float realDur)
{
	return EmuDuration(realDur * speedSetting.getValue() / 100.0);
}

void RealTime::update(const SettingLeafNode* setting) throw()
{
	resync();
}

} // namespace openmsx

