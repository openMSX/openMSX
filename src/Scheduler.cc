// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "HotKey.hh"
#include "ConsoleSource/ConsoleManager.hh"
#include "ConsoleSource/CommandController.hh"
#include "Mixer.hh"
#include <cassert>
#include <SDL/SDL.h>
#include <algorithm>



// Scheduler

Scheduler::Scheduler()
{
	paused = false;
	noSound = false;
	runningScheduler = true;
	exitScheduler = false;
	cpu = MSXCPU::instance();
	
	EventDistributor::instance()->registerAsyncListener(SDL_QUIT, this);
	CommandController::instance()->registerCommand(quitCmd, "quit");
	CommandController::instance()->registerCommand(muteCmd, "mute");
	HotKey::instance()->registerHotKeyCommand(SDLK_F12, "quit");
	HotKey::instance()->registerHotKeyCommand(SDLK_F11, "mute");
}

Scheduler::~Scheduler()
{
}

Scheduler* Scheduler::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new Scheduler();
	}
	return oneInstance;
}
Scheduler *Scheduler::oneInstance = NULL;


void Scheduler::setSyncPoint(const EmuTime &time, Schedulable* device, int userData)
{
	schedMutex.grab();
	
	PRT_DEBUG("Sched: registering " << device->getName() << " for emulation at " << time);
	PRT_DEBUG("Sched:  CPU is at " << cpu->getCurrentTime());
	
	if (time < cpu->getTargetTime())
		cpu->setTargetTime(time);
	syncPoints.push_back(SynchronizationPoint (time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
	
	schedMutex.release();
}

bool Scheduler::removeSyncPoint(Schedulable* device, int userData)
{
	bool result;
	schedMutex.grab();
	
	std::vector<SynchronizationPoint>::iterator i;
	for (i=syncPoints.begin(); i!=syncPoints.end(); i++) {
		if (((*i).getDevice() == device) &&
		     (*i).getUserData() == userData) {
			syncPoints.erase(i);
			make_heap(syncPoints.begin(), syncPoints.end());
			result = true;
			break;
		}
	}
	result = false;
	
	schedMutex.release();
	return result;
}

void Scheduler::stopScheduling()
{
	exitScheduler = true;
	runningScheduler = false;
	reschedule();
	unpause();
}

void Scheduler::reschedule()
{
	// Reschedule ASAP. We must give a device, choose MSXCPU.
	EmuTime zero;
	setSyncPoint(zero, cpu);
}

void Scheduler::scheduleEmulation()
{
	while (!exitScheduler) {
		while (runningScheduler) {
			schedMutex.grab();
			if (syncPoints.empty()) {
				// nothing scheduled, emulate CPU
				schedMutex.release();
				PRT_DEBUG ("Sched: Scheduling CPU till infinity");
				const EmuTime infinity = EmuTime(EmuTime::INFINITY);
				cpu->executeUntilTarget(infinity);
			} else {
				const SynchronizationPoint sp = *(syncPoints.begin());
				const EmuTime &time = sp.getTime();
				if (cpu->getCurrentTime() < time) {
					schedMutex.release();
					// emulate CPU till first SP, don't immediately emulate
					// device since CPU could not have reached SP
					PRT_DEBUG ("Sched: Scheduling CPU till " << time);
					cpu->executeUntilTarget(time);
				} else {
					// if CPU has reached SP, emulate the device
					pop_heap(syncPoints.begin(), syncPoints.end());
					syncPoints.pop_back();
					schedMutex.release();
					Schedulable *device = sp.getDevice();
					int userData = sp.getUserData();
					PRT_DEBUG ("Sched: Scheduling " << device->getName() << " till " << time);
					device->executeUntilEmuTime(time, userData);
					
				}
			}
		}
		pauseMutex.grab();	// grab and release mutex, if unpaused this will
		pauseMutex.release();	//  succeed else we sleep till unpaused
	}
}

void Scheduler::unpause()
{
	if (paused) {
		paused = false;
		runningScheduler = true;
		Mixer::instance()->pause(noSound);
		PRT_DEBUG("Unpaused");
		pauseMutex.release();
	}
}
void Scheduler::pause()
{
	if (!paused) {
		pauseMutex.grab();
		paused = true;
		runningScheduler = false;
		Mixer::instance()->pause(true);
		PRT_DEBUG("Paused");
	}
}
bool Scheduler::isPaused()
{
	return paused;
}

// Note: this runs in a different thread
void Scheduler::signalEvent(SDL_Event &event) {
	if (event.type == SDL_QUIT) {
		stopScheduling();
	} else {
		assert(false);
	}
}


void Scheduler::QuitCmd::execute(const std::vector<std::string> &tokens)
{
	Scheduler::instance()->stopScheduling();
}
void Scheduler::QuitCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("Use this command to stop the emulator");
}

//TODO this command belongs in Mixer instead of Scheduler
void Scheduler::MuteCmd::execute(const std::vector<std::string> &tokens)
{
	Scheduler *sch = Scheduler::instance();
	switch (tokens.size()) {
		case 1:
			sch->noSound = !sch->noSound;
			break;
		case 2:
			if (tokens[1] == "on") {
				sch->noSound = true;
				break;
			}
			if (tokens[1] == "off") {
				sch->noSound = false;
				break;
			}
		default:
			ConsoleManager::instance()->print("Syntax error");
	}
	Mixer::instance()->pause(sch->noSound||sch->isPaused());
}
void Scheduler::MuteCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("Use this command to mute/unmute the emulator");
	ConsoleManager::instance()->print(" mute:     toggle mute");
	ConsoleManager::instance()->print(" mute on:  set muted");
	ConsoleManager::instance()->print(" mute off: set unmuted");
}

