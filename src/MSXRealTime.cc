// $Id: 

#include <SDL/SDL.h>
#include "MSXRealTime.hh"


MSXRealTime::MSXRealTime(MSXConfig::Device *config) : MSXDevice(config), 
	emuRef(1000), emuOrigin(1000)	// timer in ms (rounding err!!)
{
	PRT_DEBUG("Constructing a MSXRealTime object");
	oneInstance = this;
}

MSXRealTime::~MSXRealTime()
{
	PRT_DEBUG("Destroying a MSXRealTime object");
}

MSXRealTime *MSXRealTime::instance()
{
	assert (oneInstance != NULL );
	return oneInstance;
}
MSXRealTime *MSXRealTime::oneInstance = NULL;


void MSXRealTime::reset()
{
	realRef = SDL_GetTicks();
	realOrigin = realRef;
	emuOrigin(0);
	emuRef(0);
	factor = 1;
	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}
	
void MSXRealTime::executeUntilEmuTime(const EmuTime &curEmu)
{
	unsigned int curReal = SDL_GetTicks();
	
	// Long period values, these are used for global speed corrections
	int totalReal = curReal - realOrigin;
	uint64 totalEmu = emuOrigin.getTicksTill(curEmu);
	PRT_DEBUG("RT: Total emu: " << totalEmu  << "ms  Total real: " << totalReal  << "ms");
	
	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
	uint64 emuPassed = emuRef.getTicksTill(curEmu);
	PRT_DEBUG("RT: Short emu: " << emuPassed << "ms  Short real: " << realPassed << "ms");
	
	int sleep = 0;
	catchUpTime = totalReal - totalEmu;
	if (catchUpTime < 0) {
		// we are too fast
		sleep = -catchUpTime;
	} else if (catchUpTime > MAX_CATCHUPTIME) {
		// way too slow
		int lost = catchUpTime - MAX_CATCHUPTIME;
		realOrigin += lost;
		PRT_DEBUG("RT: Emulation too slow, lost " << lost << "ms");
	}
	if ((sleep+realPassed) < MIN_REALTIME) {
		// avoid catching up too fast
		sleep = MIN_REALTIME-sleep;
	}
	if (sleep > 0) {
		PRT_DEBUG("RT: Sleeping for " << sleep << "ms");
		SDL_Delay(sleep);
	}

	// estimate current speed, values are inaccurate so take average
	float curFactor = (sleep+realPassed) / (float)emuPassed;
	factor = factor*(1-ALPHA)+curFactor*ALPHA;	// estimate with exponential average
	PRT_DEBUG("RT: Estimated speed factor (real/emu): " << factor);
	
	// adjust short period references
	realRef = curReal+sleep;	//SDL_GetTicks();
	emuRef = curEmu;

	// schedule again in future
	Scheduler::instance()->setSyncPoint(curEmu+SYNCINTERVAL, *this);
}

float MSXRealTime::getRealDuration(EmuTime time1, EmuTime time2)
{
	return time1.getDuration(time2) * factor;
}
