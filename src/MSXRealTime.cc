// $Id: 

#include <SDL/SDL.h>
#include "MSXRealTime.hh"


MSXRealTime::MSXRealTime() : emuRef(1000, 0), emuOrigin(1000, 0)	// timer in ms (rounding err!!)
{
	PRT_DEBUG("Constructing a MSXRealTime object");
	factor = 1;
	realOrigin = SDL_GetTicks();
}

MSXRealTime::~MSXRealTime()
{
	PRT_DEBUG("Destroying a MSXRealTime object");
}

MSXRealTime *MSXRealTime::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new MSXRealTime();
	}
	return oneInstance;
}
MSXRealTime *MSXRealTime::oneInstance = NULL;


void MSXRealTime::reset()
{
	realRef = SDL_GetTicks();
	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}
	
void MSXRealTime::executeUntilEmuTime(const Emutime &curEmu)
{
	unsigned int curReal = SDL_GetTicks();
	int realPassed = curReal - realRef;
	uint64 emuPassed = emuRef.getTicksTill(curEmu);
	int totalReal = curReal - realOrigin;
	uint64 totalEmu = emuOrigin.getTicksTill(curEmu);
	realRef = curReal;
	emuRef = curEmu;

	PRT_DEBUG("Total emu: " << totalEmu  << "ms  Total real: " << totalReal  << "ms");
	PRT_DEBUG("      emu: " << emuPassed << "ms        real: " << realPassed << "ms");
	catchUpTime = totalReal - totalEmu;
	
	int slept = 0;
	if (catchUpTime < 0) {
		// we are too fast
		slept = -catchUpTime;
		PRT_DEBUG("Sleeping for " << slept << "ms");
		SDL_Delay(slept);
		catchUpTime = 0;

		unsigned int test = SDL_GetTicks();
		PRT_DEBUG("*** slept: " << slept << "  time: " << test-curReal << " error: " << slept-(test-curReal));
	} else {
		// too slow
		if (catchUpTime > MAX_CATCHUPTIME) {
			// way too slow
			// TODO adjust origin's
		}
		PRT_DEBUG("Time to catch up in next frames: " << catchUpTime << "ms");
	}

	float curFactor = (slept+realPassed) / (float)emuPassed;
	factor = factor*(1-ALPHA)+curFactor*ALPHA;	// estimate with exponential average
	PRT_DEBUG("Estimated speed factor (real/emu): " << factor);
	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}

float MSXRealTime::getRealDuration(Emutime time1, Emutime time2)
{
	return time1.getDuration(time2) * factor;
}
