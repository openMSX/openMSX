// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "Mixer.hh"
#include <cassert>
#include <algorithm>
#include "EventDistributor.hh"
#include "Schedulable.hh"
#include "Renderer.hh" // TODO: Temporary?


const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
{
	paused = false;
	emulationRunning = true;
	cpu = MSXCPU::instance();
	renderer = NULL;
}

Scheduler::~Scheduler()
{
}

Scheduler* Scheduler::instance()
{
	static Scheduler* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new Scheduler();
	}
	return oneInstance;
}

void Scheduler::setSyncPoint(const EmuTime &timeStamp, Schedulable* device, int userData)
{
	const EmuTime &targetTime = cpu->getTargetTime();
	EmuTime time = (timeStamp == ASAP) ? targetTime : timeStamp;

	if (device) {
		//PRT_DEBUG("Sched: registering " << device->schedName() <<
		//          " " << userData << " for emulation at " << time);
	}

	if (time < targetTime) {
		cpu->setTargetTime(time);
	}
	syncPoints.push_back(SynchronizationPoint (time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
}

void Scheduler::removeSyncPoint(Schedulable* device, int userData)
{
	std::vector<SynchronizationPoint>::iterator i;
	for (i=syncPoints.begin(); i!=syncPoints.end(); i++) {
		if (((*i).getDevice() == device) && (*i).getUserData() == userData) {
			syncPoints.erase(i);
			make_heap(syncPoints.begin(), syncPoints.end());
			return;
		}
	}
}

void Scheduler::stopScheduling()
{
	emulationRunning = false;
	setSyncPoint(ASAP, NULL);
	unpause();
}

inline void Scheduler::emulateStep()
{
	assert(!syncPoints.empty());	// class RealTime always has one
	const SynchronizationPoint sp = syncPoints.front();
	const EmuTime &time = sp.getTime();
	if (cpu->getTargetTime() < time) {
		// first bring CPU till SP
		//  (this may set earlier SP)
		PRT_DEBUG ("Sched: Scheduling CPU till " << time);
		cpu->executeUntilTarget(time);
	} else {
		// if CPU has reached SP, emulate the device
		pop_heap(syncPoints.begin(), syncPoints.end());
		syncPoints.pop_back();
		Schedulable *device = sp.getDevice();
		int userData = sp.getUserData();
		PRT_DEBUG ("Sched: Scheduling " << device->schedName() <<
			" " << userData << " till " << time);
		device->executeUntilEmuTime(time, userData);
	}
}

const EmuTime Scheduler::scheduleEmulation()
{
	EventDistributor *eventDistributor = EventDistributor::instance();
	while (emulationRunning) {
		if (paused) {
			SDL_Delay(20); // 50 fps
			assert(renderer != NULL);
			renderer->putStoredImage();
		} else {
			emulateStep();
		}
		eventDistributor->run();
	}
	return cpu->getTargetTime();
}

void Scheduler::unpause()
{
	if (paused) {
		paused = false;
		Mixer::instance()->unmute();
	}
}

void Scheduler::pause()
{
	if (!paused) {
		paused = true;
		Mixer::instance()->mute();
	}
}

