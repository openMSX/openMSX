// $Id$

#include <SDL/SDL.h>
#include <list>
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "msxconfig.hh"


RealTime::RealTime()
{
	PRT_DEBUG("Constructing a RealTime object");
	
	MSXConfig::Config *config = MSXConfig::instance()->getConfigById("RealTime");
	syncInterval     = config->getParameterAsInt("sync_interval");
	maxCatchUpTime   = config->getParameterAsInt("max_catch_up_time");
	maxCatchUpFactor = config->getParameterAsInt("max_catch_up_factor");
	
	realRef = SDL_GetTicks();
	realOrigin = realRef;
	factor = 1;
	paused = false;
	
	HotKey::instance()->registerAsyncHotKey(SDLK_PAUSE, this);
	Scheduler::instance()->setSyncPoint(emuRef+syncInterval, this);
}

RealTime::~RealTime()
{
	PRT_DEBUG("Destroying a RealTime object");
}

RealTime *RealTime::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new RealTime();
	}
	return oneInstance;
}
RealTime *RealTime::oneInstance = NULL;


void RealTime::executeUntilEmuTime(const EmuTime &curEmu, int userData)
{
	internalSync(curEmu);
}

void RealTime::sync()
{
	Scheduler::instance()->removeSyncPoint(this);
	internalSync(MSXCPU::instance()->getCurrentTime());
}

void RealTime::internalSync(const EmuTime &curEmu)
{
	unsigned int curReal = SDL_GetTicks();
	
	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
	int emuPassed = (int)emuRef.getTicksTill(curEmu);

	if ((emuPassed>0) && (realPassed>0)) {
		// only sync if we got meaningfull values
		
		PRT_DEBUG("RT: Short emu: " << emuPassed << "ms  Short real: " << realPassed << "ms");
		
		// Long period values, these are used for global speed corrections
		int totalReal = curReal - realOrigin;
		uint64 totalEmu = emuOrigin.getTicksTill(curEmu);
		PRT_DEBUG("RT: Total emu: " << totalEmu  << "ms  Total real: " << totalReal  << "ms");
	
		int sleep = 0;
		catchUpTime = totalReal - totalEmu;
		if (catchUpTime < 0) {
			// we are too fast
			sleep = -catchUpTime;
		} else if (catchUpTime > maxCatchUpTime) {
			// way too slow
			int lost = catchUpTime - maxCatchUpTime;
			realOrigin += lost;
			PRT_DEBUG("RT: Emulation too slow, lost " << lost << "ms");
		}
		if (100*(sleep+realPassed) < maxCatchUpFactor*emuPassed) {
			// avoid catching up too fast
			sleep = (maxCatchUpFactor*emuPassed)/100 - realPassed;
		}
		if (sleep > 0) {
			PRT_DEBUG("RT: Sleeping for " << sleep << "ms");
			SDL_Delay(sleep);
		}
		
		// estimate current speed, values are inaccurate so take average
		float curFactor = (sleep+realPassed) / (float)emuPassed;
		factor = factor*(1-alpha)+curFactor*alpha;	// estimate with exponential average
		PRT_DEBUG("RT: Estimated speed factor (real/emu): " << factor);
		
		// adjust short period references
		realRef = curReal+sleep;	//SDL_GetTicks();
		emuRef = curEmu;
	}
	// schedule again in future
	Scheduler::instance()->setSyncPoint(emuRef+syncInterval, this);
}

float RealTime::getRealDuration(const EmuTime time1, const EmuTime time2)
{
	return time1.getDuration(time2) * factor;
}

// Note: this runs in a different thread
void RealTime::signalHotKey(SDLKey key) {
	if (key == SDLK_PAUSE) {
		if (paused) {
			// reset timing variables 
			realOrigin = SDL_GetTicks();
			realRef = realOrigin;
			emuOrigin = MSXCPU::instance()->getCurrentTime();
			emuRef = emuOrigin;
			
			Scheduler::instance()->unpause();
			paused = false;
		} else {
			Scheduler::instance()->pause();
			paused =true;
		}
	} else {
		assert(false);
	}
}
