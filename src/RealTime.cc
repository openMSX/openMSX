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

void RealTime::initBase()
{
	reset(EmuTime::zero);
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




const float ALPHA = 0.2; // TODO: make tuneable???


void RealTime::resync()
{
	resyncFlag = true;
}

float RealTime::doSync(const EmuTime &curEmu)
{
	if (resyncFlag) {
		reset(curEmu);
		return emuFactor;
	}

	unsigned int curReal = getTime();

	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
	int speed = 25600 / speedSetting.getValue();
	int emuPassed = (int)((speed * emuRef.getTicksTill(curEmu)) >> 8);

	PRT_DEBUG("RT: Short emu: " << emuPassed << "ms  Short real: " << realPassed << "ms");
	assert(emuPassed >= 0);
	assert(realPassed >= 0);
	// only sync if we got meaningfull values
	if ((emuPassed > 0) && (realPassed > 0)) {
		// Long period values, these are used for global speed corrections
		int totalReal = curReal - realOrigin;
		uint64 totalEmu = (speed * emuOrigin.getTicksTill(curEmu)) >> 8;
		PRT_DEBUG("RT: Total emu: " << totalEmu  << "ms  Total real: " << totalReal  << "ms");

		int sleep = 0;
		catchUpTime = totalReal - totalEmu;
		PRT_DEBUG("RT: catchUpTime: " << catchUpTime << "ms");
		if (catchUpTime < 0) {
			// we are too fast
			sleep = -catchUpTime;
		} else {
			if (catchUpTime > maxCatchUpTime) {
				// we are way too slow
				int lost = catchUpTime - maxCatchUpTime;
				realOrigin += lost;
				PRT_DEBUG("RT: Emulation too slow, lost " << lost << "ms");
			}
			if (maxCatchUpFactor * realPassed < 100 * emuPassed) {
				// we are slightly too slow, avoid catching up too fast
				sleep = (100 * emuPassed) / maxCatchUpFactor - realPassed;
				//PRT_DEBUG("RT: max catchup: " << sleep << "ms");
			}
		}
		PRT_DEBUG("RT: want to sleep " << sleep << "ms");
		sleep += (int)sleepAdjust;
		int slept, delta;
		if (sleep > 0) {
			PRT_DEBUG("RT: Sleeping for " << sleep << "ms");
			doSleep(sleep);
			slept = getTime() - curReal;
			PRT_DEBUG("RT: Realy slept for " << slept << "ms");
			delta = sleep - slept;
		} else {
			slept = 0;
			delta = 0;
		}
		sleepAdjust = sleepAdjust * (1 - ALPHA) + delta * ALPHA;
		PRT_DEBUG("RT: SleepAdjust: " << sleepAdjust);

		// estimate current speed, values are inaccurate so take average
		float curEmuFac = realPassed / (float)emuPassed;
		emuFactor = emuFactor * (1 - ALPHA) + curEmuFac * ALPHA;
		PRT_DEBUG("RT: Estimated max     speed (real/emu): " << emuFactor);

		// adjust short period references
		realRef = getTime();
		emuRef = curEmu;
	}
	return emuFactor;
}

void RealTime::reset(const EmuTime& time)
{
	reset();
	resyncFlag = false;
	realRef = realOrigin = getTime();
	emuRef  = emuOrigin  = time;
	emuFactor   = 1.0;
	sleepAdjust = 0.0;
}

} // namespace openmsx

