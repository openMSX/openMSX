// $Id: 

#include <SDL/SDL.h>
#include "MSXRealTime.hh"


MSXRealTime::MSXRealTime() : emuRef(1000, 0)	// timer in ms (rounding err!!)
{
	PRT_DEBUG("Constructing a MSXRealTime object");
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
	
void MSXRealTime::executeUntilEmuTime(const Emutime &time)
{
	unsigned int curTime = SDL_GetTicks();
	int realPassed = curTime - realRef;
	realRef = curTime;
	int emuPassed = emuRef.getTicksTill(time);
	emuRef = time;

	PRT_DEBUG("Emu  " << emuPassed << "ms    Real " << realPassed << "ms");
	int diff = emuPassed - realPassed;
	if (diff>0) {
		PRT_DEBUG("Sleeping for " << diff << " ms");
		SDL_Delay(diff);
	}

	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}
