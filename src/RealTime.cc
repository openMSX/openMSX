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

const int SYNC_INTERVAL = 500;    // ms
const int MAX_LAG       = 200000; // us


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
	return (currentRealTime + us) < (idealRealTime + realDuration);
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
		emuTime = time;
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
	
	// Schedule again in future
	EmuTimeFreq<1000> time2(time);
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





/*


void RealTime::doSync(const EmuTime &curEmu)
{
	unsigned int curReal = getRealTime();

	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
	int speed = 25600 / speedSetting.getValue();
	int emuPassed = (int)((speed * emuRef.getTicksTill(curEmu)) >> 8);

	PRT_DEBUG("RT: Short emu: " << emuPassed << "us  Short real: " << realPassed << "us");
	assert(emuPassed >= 0);
	assert(realPassed >= 0);
	// only sync if we got meaningfull values
	if ((emuPassed > 0) && (realPassed > 0)) {
		// Long period values, these are used for global speed corrections
		int totalReal = curReal - realOrigin;
		uint64 totalEmu = (speed * emuOrigin.getTicksTill(curEmu)) >> 8;
		PRT_DEBUG("RT: Total emu: " << totalEmu  << "us  Total real: " << totalReal  << "us");

		int sleep = 0;
		catchUpTime = totalReal - totalEmu;
		PRT_DEBUG("RT: catchUpTime: " << catchUpTime << "us");
		if (catchUpTime < 0) {
			// we are too fast
			sleep = -catchUpTime;
		} else {
			if (catchUpTime > maxCatchUpTime) {
				// we are way too slow
				int lost = catchUpTime - maxCatchUpTime;
				realOrigin += lost;
				PRT_DEBUG("RT: Emulation too slow, lost " << lost << "us");
			}
			if (maxCatchUpFactor * realPassed < 100 * emuPassed) {
				// we are slightly too slow, avoid catching up too fast
				sleep = (100 * emuPassed) / maxCatchUpFactor - realPassed;
				//PRT_DEBUG("RT: max catchup: " << sleep << "us");
			}
		}
		PRT_DEBUG("RT: want to sleep " << sleep << "us");
		sleep += (int)sleepAdjust;
		int slept, delta;
		if (sleep > 0) {
			PRT_DEBUG("RT: Sleeping for " << sleep << "us");
			doSleep(sleep);
			slept = getRealTime() - curReal;
			PRT_DEBUG("RT: Realy slept for " << slept << "us");
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
		realRef = getRealTime();
		emuRef = curEmu;
	}
	return emuFactor;
}

*/
