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
	: sem(1)
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
	sem.down();
	syncPoints.push_back(SynchronizationPoint (time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
	sem.up();
}

void Scheduler::removeSyncPoint(Schedulable* device, int userData)
{
	sem.down();
	for (vector<SynchronizationPoint>::iterator it = syncPoints.begin();
	     it != syncPoints.end();
	     ++it) {
		SynchronizationPoint& sp = *it;
		if ((sp.getDevice() == device) && 
		    (sp.getUserData() == userData)) {
			syncPoints.erase(it);
			make_heap(syncPoints.begin(), syncPoints.end());
			break;
		}
	}
	sem.up();
}

void Scheduler::stopScheduling()
{
	emulationRunning = false;
	setSyncPoint(ASAP, NULL);
	unpause();
}

void Scheduler::scheduleDevices(const EmuTime &limit)
{
	sem.down();
	assert(!syncPoints.empty());	// class RealTime always has one
	SynchronizationPoint sp = syncPoints.front();
	EmuTime time = sp.getTime();
	while (time <= limit) {
		// emulate the device
		pop_heap(syncPoints.begin(), syncPoints.end());
		syncPoints.pop_back();
		sem.up();
		Schedulable *device = sp.getDevice();
		int userData = sp.getUserData();
		PRT_DEBUG ("Sched: Scheduling_2 " << device->schedName() <<
			" " << userData << " till " << time);
		device->executeUntilEmuTime(time, userData);

		sem.down();
		sp = syncPoints.front();
		time = sp.getTime();
	}
	sem.up();
}

inline void Scheduler::emulateStep()
{
	sem.down();
	assert(!syncPoints.empty());	// class RealTime always has one
	const SynchronizationPoint sp = syncPoints.front();
	const EmuTime &time = sp.getTime();
	if (cpu->getTargetTime() < time) {
		sem.up();
		// first bring CPU till SP
		//  (this may set earlier SP)
		PRT_DEBUG ("Sched: Scheduling CPU till " << time);
		cpu->executeUntilTarget(time);
	} else {
		// if CPU has reached SP, emulate the device
		pop_heap(syncPoints.begin(), syncPoints.end());
		syncPoints.pop_back();
		sem.up();
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

