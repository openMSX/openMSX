// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "Mixer.hh"
#include <cassert>
#include <algorithm>
#include "EventDistributor.hh"
#include "Schedulable.hh"


const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
{
	paused = false;
	needBlock = false;
	emulationRunning = true;
	cpu = MSXCPU::instance();
}

Scheduler::~Scheduler()
{
}

Scheduler* Scheduler::instance()
{
	static Scheduler* oneInstance = NULL;
	if (oneInstance == NULL )
		oneInstance = new Scheduler();
	return oneInstance;
}

void Scheduler::setSyncPoint(const EmuTime &timeStamp, Schedulable* device, int userData)
{
	schedMutex.grab();
	
	const EmuTime &targetTime = cpu->getTargetTime();
	EmuTime time = (timeStamp == ASAP) ? targetTime : timeStamp;
	
	if (device) { 
		PRT_DEBUG("Sched: registering " << device->schedName() << 
		          " " << userData << " for emulation at " << time);
	}

	if (time < targetTime) {
		cpu->setTargetTime(time);
	}
	syncPoints.push_back(SynchronizationPoint (time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());

	pauseCond.signal();
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
	emulationRunning = false;
	setSyncPoint(ASAP, NULL);
	unpause();
}

const EmuTime Scheduler::scheduleEmulation()
{
	while (emulationRunning) {
		schedMutex.grab();
		assert (!syncPoints.empty());	// class RealTime always has one
		const SynchronizationPoint sp = syncPoints.front();
		const EmuTime &time = sp.getTime();
		if (cpu->getTargetTime() < time) {
			schedMutex.release();
			// first bring CPU till SP 
			//  (this may set earlier SP)
			if (!paused) {
				PRT_DEBUG ("Sched: Scheduling CPU till " << time);
				cpu->executeUntilTarget(time);
			} else {
				needBlock = true;
			}
		} else {
			// if CPU has reached SP, emulate the device
			pop_heap(syncPoints.begin(), syncPoints.end());
			syncPoints.pop_back();
			schedMutex.release();
			Schedulable *device = sp.getDevice();
			int userData = sp.getUserData();
			PRT_DEBUG ("Sched: Scheduling " << device->schedName() << 
				   " " << userData << " till " << time);
			device->executeUntilEmuTime(time, userData);
		}
		if (needBlock) {
			while (pauseCond.waitTimeout(10)!=0)
				EventDistributor::instance()->run();
		} else {
			EventDistributor::instance()->run();
		}
	}
	return cpu->getTargetTime();
}

void Scheduler::unpause()
{
	if (paused) {
		paused = false;
		needBlock = false;
		Mixer::instance()->unmute();
		pauseCond.signal();
	}
}

void Scheduler::pause()
{
	if (!paused) {
		paused = true;
		Mixer::instance()->mute();
	}
}

bool Scheduler::isPaused()
{
	return paused;
}

