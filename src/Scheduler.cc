// $Id$

#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "HotKey.hh"
#include "Mixer.hh"
#include <cassert>
#include <SDL/SDL.h>


// Schedulable 

const std::string &Schedulable::getName()
{
	return defaultName;
}
const std::string Schedulable::defaultName = "no name";


// Scheduler

Scheduler::Scheduler()
{
	pauseMutex = SDL_CreateMutex();
	paused = false;
	noSound = false;
	runningScheduler = true;
	EventDistributor::instance()->registerAsyncListener(SDL_QUIT, this);
	HotKey::instance()->registerAsyncHotKey(SDLK_F12, this);
	HotKey::instance()->registerAsyncHotKey(SDLK_F11, this);
}

Scheduler::~Scheduler()
{
	SDL_DestroyMutex(pauseMutex);
}

Scheduler* Scheduler::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new Scheduler();
	}
	return oneInstance;
}
Scheduler *Scheduler::oneInstance = NULL;


void Scheduler::setSyncPoint(const EmuTime &time, Schedulable &device) 
{
	PRT_DEBUG("Sched: registering " << device.getName() << " for emulation at " << time);
	PRT_DEBUG("Sched:  CPU is at " << MSXCPU::instance()->getCurrentTime());
	assert (time >= MSXCPU::instance()->getCurrentTime());
	if (time < MSXCPU::instance()->getTargetTime())
		MSXCPU::instance()->setTargetTime(time);
	syncPoints.insert(SynchronizationPoint (time, device)); // automatically sorted
}

const Scheduler::SynchronizationPoint &Scheduler::getFirstSP()
{
	assert (!syncPoints.empty());
	return *(syncPoints.begin());
}

void Scheduler::removeFirstSP()
{
	assert (!syncPoints.empty());
	syncPoints.erase(syncPoints.begin());
}

void Scheduler::stopScheduling()
{
	runningScheduler=false;
	// We set current time as SP, this means reschedule as sson as possible.
	// We must give a device, we choose MSXCPU.
	EmuTime now = MSXCPU::instance()->getCurrentTime();
	setSyncPoint(now, *(MSXCPU::instance())); 
}

void Scheduler::scheduleEmulation()
{
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

		SDL_mutexP(pauseMutex); // grab and release mutex, if unpaused this will
		SDL_mutexV(pauseMutex); //  succeed else we sleep till unpaused

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
				Schedulable *device = &(sp.getDevice());
				PRT_DEBUG ("Sched: Scheduling " << device->getName() << " till " << time);
				device->executeUntilEmuTime(time);
				removeFirstSP();
			}
		}
	}
}

void Scheduler::unpause()
{
	if (paused) { 
		paused = false;
		SDL_mutexV(pauseMutex);	// release mutex;
		Mixer::instance()->pause(noSound);
		PRT_DEBUG("Unpaused");
	}
}
void Scheduler::pause()
{
	if (!paused) { 
		paused = true;
		SDL_mutexP(pauseMutex);	// grab mutex
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

