// $Id$

#include "RealTimeSDL.hh"
#include <SDL/SDL.h>

namespace openmsx {

const float ALPHA = 0.2; // TODO: make tuneable???


RealTimeSDL::RealTimeSDL()
{
	reset(EmuTime::zero);
}

RealTimeSDL::~RealTimeSDL()
{
}

void RealTimeSDL::resync()
{
	resyncFlag = true;
}

float RealTimeSDL::doSync(const EmuTime &curEmu)
{
	if (resyncFlag) {
		reset(curEmu);
		return emuFactor;
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
			SDL_Delay(sleep);
			slept = SDL_GetTicks() - curReal;
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
		realRef = SDL_GetTicks();
		emuRef = curEmu;
	}
	return emuFactor;
}

void RealTimeSDL::reset(const EmuTime &time)
{
	resyncFlag = false;
	realRef = realOrigin = SDL_GetTicks();
	emuRef  = emuOrigin  = time;
	emuFactor   = 1.0;
	sleepAdjust = 0.0;
}

} // namespace openmsx

