// $Id$

#include "Keys.hh"
#include "RealTime.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "Scheduler.hh"


RealTime::RealTime()
{
	Config *config = MSXConfig::instance()->getConfigById("RealTime");
	syncInterval     = config->getParameterAsInt("sync_interval");
	maxCatchUpTime   = config->getParameterAsInt("max_catch_up_time");
	maxCatchUpFactor = config->getParameterAsInt("max_catch_up_factor");
	
	scheduler = Scheduler::instance();
	speed = 256;
	paused = false;
	throttle = true;
	EmuTime zero;
	reset(zero);
	scheduler->setSyncPoint(emuRef+syncInterval, this);
	
	CommandController::instance()->registerCommand(&pauseCmd, "pause");
	CommandController::instance()->registerCommand(&throttleCmd, "throttle");
	CommandController::instance()->registerCommand(&speedCmd, "speed");
	HotKey::instance()->registerHotKeyCommand(Keys::K_PAUSE, "pause");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F9, "throttle");
}

RealTime::~RealTime()
{
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_PAUSE, "pause");
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F9, "throttle");
	CommandController::instance()->unregisterCommand(&pauseCmd, "pause");
	CommandController::instance()->unregisterCommand(&throttleCmd, "throttle");
	CommandController::instance()->unregisterCommand(&speedCmd, "speed");
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

void RealTime::internalSync(const EmuTime &curEmu)
{
	if (!throttle) {
		reset(curEmu);
		return;
	}
	
	unsigned int curReal = SDL_GetTicks();
	
	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
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
	scheduler->setSyncPoint(time + syncInterval, this);
}

float RealTime::getRealDuration(const EmuTime &time1, const EmuTime &time2)
{
	return (time2 - time1).toFloat() * totalFactor;
}

void RealTime::reset(const EmuTime &time)
{
	realRef = realOrigin = SDL_GetTicks();
	emuRef  = emuOrigin  = time;
	emuFactor   = 1.0;
	totalFactor = 1.0;
	sleepAdjust = 0.0;
}

void RealTime::PauseCmd::execute(const std::vector<std::string> &tokens,
                                 const EmuTime &time)
{
	Scheduler *sch = Scheduler::instance();
	switch (tokens.size()) {
	case 1:
		if (sch->isPaused()) {
			RealTime::instance()->reset(time); 
			sch->unpause();
		} else {
			sch->pause();
		}
		break;
	case 2:
		if (tokens[1] == "on") {
			sch->pause();
			break;
		}
		if (tokens[1] == "off") {
			RealTime::instance()->reset(time); 
			sch->unpause();
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}
void RealTime::PauseCmd::help(const std::vector<std::string> &tokens) const
{
	print("Use this command to pause/unpause the emulator");
	print(" pause:     toggle pause");
	print(" pause on:  pause emulation");
	print(" pause off: unpause emulation");
}

void RealTime::ThrottleCmd::execute(const std::vector<std::string> &tokens,
                                    const EmuTime &time)
{
	RealTime *rt = RealTime::instance();
	switch (tokens.size()) {
	case 1:
		rt->throttle = !rt->throttle;
		break;
	case 2:
		if (tokens[1] == "on") {
			rt->throttle = true;
			break;
		}
		if (tokens[1] == "off") {
			rt->throttle = false;
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}
void RealTime::ThrottleCmd::help(const std::vector<std::string> &tokens) const
{
	print("This command turns speed throttling on/off");
	print(" throttle:     toggle throttling");
	print(" throttle on:  run emulation on normal speed");
	print(" throttle off: run emulation on maximum speed");
}

void RealTime::SpeedCmd::execute(const std::vector<std::string> &tokens,
                                 const EmuTime &time)
{
	RealTime *rt = RealTime::instance();
	switch (tokens.size()) {
	case 1: {
		char message[100];
		sprintf(message, "Current speed: %d", 25600/rt->speed);
		print(std::string(message));
		break;
	}
	case 2: {
		int tmp = strtol(tokens[1].c_str(), NULL, 0);
		if (tmp > 0) {
			rt->speed = 25600 / tmp;
			rt->reset(time);
		} else {
			throw CommandException("Illegal argument");
		}
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void RealTime::SpeedCmd::help(const std::vector<std::string> &tokens) const
{
	print("This command controls the emulation speed");
	print("A higher value means faster emualtion, normal speed is 100.");
	print(" speed:     : shows current speed");
	print(" speed <num>: sets new speed");
}

