// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "HotKey.hh"
#include "Console.hh"
#include "Mixer.hh"
#include <cassert>
#include <SDL/SDL.h>
#include <algorithm>


// Schedulable

Schedulable::Schedulable() {
}

const std::string &Schedulable::getName()
{
	return defaultName;
}
const std::string Schedulable::defaultName = "no name";


// Scheduler

Scheduler::Scheduler()
{
	paused = false;
	noSound = false;
	runningScheduler = true;
	exitScheduler = false;
	EventDistributor::instance()->registerAsyncListener(SDL_QUIT, this);
	HotKey::instance()->registerAsyncHotKey(SDLK_F12, this);
	HotKey::instance()->registerAsyncHotKey(SDLK_F11, this);
	Console::instance()->registerCommand(this, "quit");
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


void Scheduler::setSyncPoint(const EmuTime &time, Schedulable* device, int userData = 0)
{
	PRT_DEBUG("Sched: registering " << device->getName() << " for emulation at " << time);
	PRT_DEBUG("Sched:  CPU is at " << MSXCPU::instance()->getCurrentTime());
	//assert (time >= MSXCPU::instance()->getCurrentTime());
	if (time < MSXCPU::instance()->getTargetTime())
		MSXCPU::instance()->setTargetTime(time);
	syncPoints.push_back(SynchronizationPoint (time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
}

bool Scheduler::removeSyncPoint(Schedulable* device, int userData = 0)
{
	std::vector<SynchronizationPoint>::iterator i;
	for (i=syncPoints.begin(); i!=syncPoints.end(); i++) {
		if (((*i).getDevice() == device) &&
		     (*i).getUserData() == userData) {
			syncPoints.erase(i);
			make_heap(syncPoints.begin(), syncPoints.end());
			return true;
		}
	}
	return false;
}

const Scheduler::SynchronizationPoint &Scheduler::getFirstSP()
{
	assert (!syncPoints.empty());
	return *(syncPoints.begin());
}

void Scheduler::removeFirstSP()
{
	assert (!syncPoints.empty());
	pop_heap(syncPoints.begin(), syncPoints.end());
	syncPoints.pop_back();
}

void Scheduler::stopScheduling()
{
	exitScheduler = true;
	runningScheduler = false;
	// We set current time as SP, this means reschedule as sson as possible.
	// We must give a device, we choose MSXCPU.
	EmuTime now = MSXCPU::instance()->getCurrentTime();
	setSyncPoint(now, MSXCPU::instance());
}

void Scheduler::scheduleEmulation()
{
	while (!exitScheduler) {
		while (runningScheduler) {

// some test stuff for joost, please leave for a few
//	std::cerr << "foo" << std::endl;
//	std::ifstream quakef("starquake/quake");
//	byte quakeb[128];
//	quakef.read(quakeb, 127);
//	EmuTime foo;
//	std::cerr << "foo" << std::endl;
//	for (int joosti=0;joosti<127;joosti++)
//	{
//		MSXMotherBoard::instance()->writeMem(0x8000+joosti,quakeb[joosti],foo);
//	}
//	quakef.close();

			if (syncPoints.empty()) {
				// nothing scheduled, emulate CPU
				PRT_DEBUG ("Sched: Scheduling CPU till infinity");
				const EmuTime infinity = EmuTime(EmuTime::INFINITY);
				MSXCPU::instance()->executeUntilTarget(infinity);
			} else {
				const SynchronizationPoint &sp = getFirstSP();
				const EmuTime &time = sp.getTime();
				if (MSXCPU::instance()->getCurrentTime() < time) {
					// emulate CPU till first SP, don't immediately emulate
					// device since CPU could not have reached SP
					PRT_DEBUG ("Sched: Scheduling CPU till " << time);
					MSXCPU::instance()->executeUntilTarget(time);
				} else {
					// if CPU has reached SP, emulate the device
					Schedulable *device = sp.getDevice();
					int userData = sp.getUserData();
					PRT_DEBUG ("Sched: Scheduling " << device->getName() << " till " << time);
					device->executeUntilEmuTime(time, userData);
					removeFirstSP();
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

// Note: this runs in a different thread
void Scheduler::signalEvent(SDL_Event &event) {
	if (event.type == SDL_QUIT) {
		stopScheduling();
	} else {
		assert(false);
	}
}

// Note: this runs in a different thread
void Scheduler::signalHotKey(SDLKey key) {
	if (key == SDLK_F12) {
		stopScheduling();
		unpause();
	} else if (key == SDLK_F11) {
		noSound = !noSound;
		Mixer::instance()->pause(noSound||paused);
	} else {
		assert(false);
	}
}

void Scheduler::ConsoleCallback(char *string)
{
  // only quit is registered :-)
  stopScheduling();
  unpause();
}

void Scheduler::ConsoleHelp(char *string)
{
  //nothing for the moment
}
