// $Id: 

#include <SDL/SDL.h>
#include "MSXRealTime.hh"


MSXRealTime::MSXRealTime() : emuRef(1000, 0)	// timer in ms (rounding err!!)
{
	PRT_DEBUG("Constructing a MSXRealTime object");
	factor = 1;
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
	realRef = curReal;
	int emuPassed = emuRef.getTicksTill(curEmu);
	emuRef = curEmu;

	int slept = 0;
	PRT_DEBUG("Emu  " << emuPassed << "ms    Real " << realPassed << "ms");
	catchUpTime += realPassed - emuPassed;
	if (catchUpTime < 0) {
		slept = -catchUpTime;
		PRT_DEBUG("Sleeping for " << slept << "ms");
		SDL_Delay(slept);
		catchUpTime = 0;
	} else {
		if (catchUpTime > MAX_CATCHUPTIME) {
			PRT_DEBUG("Emualtion too slow, lost " << catchUpTime-MAX_CATCHUPTIME << "ms");
			catchUpTime = MAX_CATCHUPTIME;
		}
		PRT_DEBUG("Time to catch up in next frames: " << catchUpTime << "ms");
	}

	float curFactor = (slept+realPassed) / emuPassed;
	factor = factor*(1-ALPHA)+curFactor*ALPHA;	// estimate with exponential average
	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}

float MSXRealTime::getRealDuration(Emutime time1, Emutime time2)
{
	return time1.getDuration(time2) * factor;
}
