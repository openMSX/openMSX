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
	
void MSXRealTime::executeUntilEmuTime(const Emutime &time)
{
	unsigned int curTime = SDL_GetTicks();
	int realPassed = curTime - realRef;
	realRef = curTime;
	int emuPassed = emuRef.getTicksTill(time);
	emuRef = time;
	float curFactor = realPassed / emuPassed;
	factor = factor*(1-ALPHA)+curFactor*ALPHA;

	PRT_DEBUG("Emu  " << emuPassed << "ms    Real " << realPassed << "ms");
	int diff = emuPassed - realPassed;
	if (diff>0) {
	  if (catchUpTime){
	    if (catchUpTime>=diff){
	      catchUpTime-=diff;
	      PRT_DEBUG("Catching up " << diff << " ms");
	    } else {
	      diff-=catchUpTime;
	      PRT_DEBUG("Catched up " << catchUpTime << " ms");
	      catchUpTime=0;
	      PRT_DEBUG("Sleeping for " << diff << " ms");
	      SDL_Delay(diff);
	    }
	  } else {
	    PRT_DEBUG("Sleeping for " << diff << " ms");
	    SDL_Delay(diff);
	  }
	} else {
		if (catchUpTime<MAX_CATCHUPTIME) catchUpTime-=diff; // increasing catchUpTime
		PRT_DEBUG("Time we need to catch up in the next frames : " << catchUpTime << " ms");
	}

	Scheduler::instance()->setSyncPoint(emuRef+SYNCINTERVAL, *this);
}

float MSXRealTime::getRealDuration(Emutime time1, Emutime time2)
{
	// TODO make better estimations
	float emuDuration = time1.getDuration(time2);
	float adjust = (factor<1) ? 1 : factor;	// MAX(1,factor)
	return emuDuration*adjust;
}
