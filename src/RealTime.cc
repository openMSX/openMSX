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
	PRT_DEBUG("Constructing a RealTime object");
	
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("RealTime");
	syncInterval     = config->getParameterAsInt("sync_interval");
	maxCatchUpTime   = config->getParameterAsInt("max_catch_up_time");
	maxCatchUpFactor = config->getParameterAsInt("max_catch_up_factor");
	
	scheduler = Scheduler::instance();
	cpu = MSXCPU::instance();
	speed = 256;
	paused = false;
	throttle = true;
	resetTiming();
	scheduler->setSyncPoint(emuRef+syncInterval, this);
	
	CommandController::instance()->registerCommand(pauseCmd, "pause");
	CommandController::instance()->registerCommand(throttleCmd, "throttle");
	CommandController::instance()->registerCommand(speedCmd, "speed");
	HotKey::instance()->registerHotKeyCommand(Keys::K_PAUSE, "pause");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F9, "throttle");
}

RealTime::~RealTime()
{
	PRT_DEBUG("Destroying a RealTime object");
	
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_PAUSE, "pause");
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F9, "throttle");
	CommandController::instance()->unregisterCommand("pause");
	CommandController::instance()->unregisterCommand("throttle");
	CommandController::instance()->unregisterCommand("speed");
}

RealTime *RealTime::instance()
{
	static RealTime* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new RealTime();
	return oneInstance;
}


void RealTime::executeUntilEmuTime(const EmuTime &curEmu, int userData)
{
	internalSync(curEmu);
}

const std::string &RealTime::schedName()
{
	static const std::string name("RealTime");
	return name;
}


void RealTime::sync()
{
	scheduler->removeSyncPoint(this);
	internalSync(cpu->getCurrentTime());
}

void RealTime::internalSync(const EmuTime &curEmu)
{
	if (!throttle) {
		resetTiming();
		return;
	}
	
	unsigned int curReal = SDL_GetTicks();
	
	// Short period values, inaccurate but we need them to estimate our current speed
	int realPassed = curReal - realRef;
	int emuPassed = (int)((speed * emuRef.getTicksTill(curEmu)) >> 8);

	if ((emuPassed>0) && (realPassed>0)) {
		// only sync if we got meaningfull values
		
		PRT_DEBUG("RT: Short emu: " << emuPassed << "ms  Short real: " << realPassed << "ms");
		
		// Long period values, these are used for global speed corrections
		int totalReal = curReal - realOrigin;
		uint64 totalEmu = (speed * emuOrigin.getTicksTill(curEmu)) >> 8;
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
		if (maxCatchUpFactor*(sleep+realPassed) < 100*emuPassed) {
			// avoid catching up too fast
			sleep = (100*emuPassed)/maxCatchUpFactor - realPassed;
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
	scheduler->setSyncPoint(emuRef+syncInterval, this);
}

float RealTime::getRealDuration(const EmuTime &time1, const EmuTime &time2)
{
	return (time2 - time1).toFloat() * factor;
}

void RealTime::resetTiming()
{
	realRef = realOrigin = SDL_GetTicks();
	emuRef  = emuOrigin  = cpu->getCurrentTime();
	factor  = 1;
}

void RealTime::PauseCmd::execute(const std::vector<std::string> &tokens)
{
	Scheduler *sch = Scheduler::instance();
	switch (tokens.size()) {
	case 1:
		if (sch->isPaused()) {
			RealTime::instance()->resetTiming(); 
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
			RealTime::instance()->resetTiming(); 
			sch->unpause();
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}
void RealTime::PauseCmd::help   (const std::vector<std::string> &tokens)
{
	print("Use this command to pause/unpause the emulator");
	print(" pause:     toggle pause");
	print(" pause on:  pause emulation");
	print(" pause off: unpause emulation");
}

void RealTime::ThrottleCmd::execute(const std::vector<std::string> &tokens)
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
void RealTime::ThrottleCmd::help   (const std::vector<std::string> &tokens)
{
	print("This command turns speed throttling on/off");
	print(" throttle:     toggle throttling");
	print(" throttle on:  run emulation on normal speed");
	print(" throttle off: run emulation on maximum speed");
}

void RealTime::SpeedCmd::execute(const std::vector<std::string> &tokens)
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
			rt->resetTiming();
		} else {
			throw CommandException("Illegal argument");
		}
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void RealTime::SpeedCmd::help   (const std::vector<std::string> &tokens)
{
	print("This command controls the emulation speed");
	print("A higher value means faster emualtion, normal speed is 100.");
	print(" speed:     : shows current speed");
	print(" speed <num>: sets new speed");
}

