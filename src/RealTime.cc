// $Id$

#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "RealTimeSDL.hh"
#include "RealTimeRTC.hh"

#include <cassert>

#ifndef	NO_LINUX_RTC
#define HAVE_RTC	// TODO check this in configure
#endif


namespace openmsx {

const int SYNC_INTERVAL = 50;


RealTime::RealTime()
	: speedSetting("speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000),
	  throttleSetting("throttle", "controls speed throttling", true),
	  pauseSetting(Scheduler::instance()->getPauseSetting()),
	  powerSetting(Scheduler::instance()->getPowerSetting())
{
	speedSetting.addListener(this);
	pauseSetting.addListener(this);
	powerSetting.addListener(this);
	
	maxCatchUpTime = 2000;	// ms
	maxCatchUpFactor = 105; // %
	try {
		Config *config = MSXConfig::instance()->getConfigById("RealTime");
		maxCatchUpTime = config->getParameterAsInt("max_catch_up_time", maxCatchUpTime);
		maxCatchUpFactor = config->getParameterAsInt("max_catch_up_factor", maxCatchUpFactor);
	} catch (ConfigException &e) {
		// no Realtime section
	}
	
	Scheduler::instance()->setSyncPoint(Scheduler::ASAP, this);
}

RealTime::~RealTime()
{
	Scheduler::instance()->removeSyncPoint(this);
	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
	speedSetting.removeListener(this);
}

RealTime *RealTime::instance()
{
	static RealTime* oneInstance = NULL;
	if (oneInstance == NULL) {
#ifdef HAVE_RTC
		oneInstance = RealTimeRTC::create();
#endif
		if (oneInstance == NULL) {
			oneInstance = new RealTimeSDL();
		}
	}
	return oneInstance;
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
	Scheduler::instance()->removeSyncPoint(this);
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
	Scheduler::instance()->setSyncPoint(time2 + SYNC_INTERVAL, this);
	
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

void RealTime::update(const SettingLeafNode* setting)
{
	resync();
}

} // namespace openmsx

