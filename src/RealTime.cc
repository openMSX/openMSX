// $Id$

#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "CommandController.hh"
#include "Scheduler.hh"

const int SYNC_INTERVAL = 50;


RealTime::RealTime()
	: throttleSetting("throttle", "controls speed throttling", true)
{
	// default values
	maxCatchUpFactor = 105; // %
	maxCatchUpTime = 2000;	// ms
	try {
		Config *config = MSXConfig::instance()->getConfigById("RealTime");
		if (config->hasParameter("max_catch_up_time")) {
			maxCatchUpTime =
				config->getParameterAsInt("max_catch_up_time");
		}
		if (config->hasParameter("max_catch_up_factor")) {
			maxCatchUpFactor =
				config->getParameterAsInt("max_catch_up_factor");
		}
	} catch (MSXException &e) {
		// no Realtime section
	}
	
	scheduler = Scheduler::instance();
	// Synchronize counters as soon as emulation actually starts.
	resyncFlag = true;
	scheduler->setSyncPoint(Scheduler::ASAP, this);
	// Safeguard against uninitialised values.
	// Should not be necessary because resync will occur first.
	EmuTime zero;
	reset(zero);
}

RealTime::~RealTime()
{
}

RealTime *RealTime::instance()
{
	static RealTime oneInstance;
	return &oneInstance;
}

void RealTime::executeUntilEmuTime(const EmuTime &curEmu, int userData)
{
	internalSync(curEmu);
}

const std::string &RealTime::schedName() const
{
	static const std::string name("RealTime");
	return name;
}

float RealTime::sync(const EmuTime &time)
{
	scheduler->removeSyncPoint(this);
	internalSync(time);
	//PRT_DEBUG("RT: user sync " << time);
	return emuFactor;
}

void RealTime::resync()
{
	resyncFlag = true;
}

void RealTime::internalSync(const EmuTime &curEmu)
{
	if (!throttleSetting.getValue()) {
		// no throttling
		reset(curEmu);
		return;
	}
	
	// Resynchronize EmuTime and real time?
	if (resyncFlag) {
		reset(curEmu);
		resyncFlag = false;
	}
	
	unsigned int curReal = SDL_GetTicks();
	
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
		} else if (catchUpTime > maxCatchUpTime) {
			// we are way too slow
			int lost = catchUpTime - maxCatchUpTime;
			realOrigin += lost;
			PRT_DEBUG("RT: Emulation too slow, lost " << lost << "ms");
		} else if (maxCatchUpFactor * (sleep + realPassed) < 100 * emuPassed) {
			// we are slightly too slow, avoid catching up too fast
			sleep = (100 * emuPassed) / maxCatchUpFactor - realPassed;
			//PRT_DEBUG("RT: max catchup: " << sleep << "ms");
		}
		PRT_DEBUG("RT: want to sleep " << sleep << "ms");
		sleep += (int)sleepAdjust;
		int slept;
		if (sleep > 0) {
			PRT_DEBUG("RT: Sleeping for " << sleep << "ms");
			SDL_Delay(sleep);
			slept = SDL_GetTicks() - curReal;
			PRT_DEBUG("RT: Realy slept for " << slept << "ms");
			int delta = sleep - slept;
			sleepAdjust = sleepAdjust * (1 - alpha) + delta * alpha;
			PRT_DEBUG("RT: SleepAdjust: " << sleepAdjust);
		} else {
			slept = 0;
		}
		
		// estimate current speed, values are inaccurate so take average
		float curTotalFac = (slept + realPassed) / (float)emuPassed;
		totalFactor = totalFactor * (1 - alpha) + curTotalFac * alpha;
		PRT_DEBUG("RT: Estimated current speed (real/emu): " << totalFactor);
		float curEmuFac = realPassed / (float)emuPassed;
		emuFactor = emuFactor * (1 - alpha) + curEmuFac * alpha;
		PRT_DEBUG("RT: Estimated max     speed (real/emu): " << emuFactor);
		
		// adjust short period references
		realRef = SDL_GetTicks();
		emuRef = curEmu;
	} 
	// schedule again in future
	EmuTimeFreq<1000> time(curEmu);
	scheduler->setSyncPoint(time + SYNC_INTERVAL, this);
}

float RealTime::getRealDuration(const EmuTime &time1, const EmuTime &time2)
{
	return (time2 - time1).toFloat() * totalFactor;
}

EmuDuration RealTime::getEmuDuration(float realDur)
{
	return EmuDuration(realDur / totalFactor);
}

void RealTime::reset(const EmuTime &time)
{
	realRef = realOrigin = SDL_GetTicks();
	emuRef  = emuOrigin  = time;
	emuFactor   = 1.0;
	totalFactor = 1.0;
	sleepAdjust = 0.0;
}

RealTime::PauseSetting::PauseSetting()
	: BooleanSetting("pause", "pauses the emulation", false)
{
}

bool RealTime::PauseSetting::checkUpdate(bool newValue)
{
	if (newValue) {
		Scheduler::instance()->pause();
	} else {
		RealTime::instance()->resync();
		Scheduler::instance()->unpause();
	}
	return true;
}

RealTime::SpeedSetting::SpeedSetting()
	: IntegerSetting("speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000)
{
}

bool RealTime::SpeedSetting::checkUpdate(int newValue)
{
	RealTime::instance()->resync();
	return true;
}
