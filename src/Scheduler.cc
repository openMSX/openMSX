// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "Mixer.hh"
#include <cassert>
#include <algorithm>
#include "EventDistributor.hh"
#include "Schedulable.hh"
#include "RealTime.hh"
#include "MSXMotherBoard.hh"
#include "Leds.hh"
#include "Renderer.hh" // TODO: Temporary?


namespace openmsx {

const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
	: sem(1),
	  pauseSetting("pause", "pauses the emulation", false),
	  powerSetting("power", "turn power on/off", false)
{
	paused = false;
	emulationRunning = true;
	cpu = MSXCPU::instance();
	cpu->init(this);
	renderer = NULL;
	
	pauseSetting.addListener(this);
	powerSetting.addListener(this);
}

Scheduler::~Scheduler()
{
	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
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
	if (paused || !powerSetting.getValue()) {
		EventDistributor::instance()->notify();
	}
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
	setSyncPoint(ASAP, NULL);	// dummy device
	unpause();
}

void Scheduler::schedule(const EmuTime& limit)
{
	EventDistributor *eventDistributor = EventDistributor::instance();
	while (emulationRunning) {
		sem.down();
		assert(!syncPoints.empty());	// class RealTime always has one
		const SynchronizationPoint sp = syncPoints.front();
		const EmuTime& time = sp.getTime();
		if (limit < time) {
			sem.up();
			return;
		}
		if (cpu->getTargetTime() < time) {
			sem.up();
			if (!paused && powerSetting.getValue()) {
				// first bring CPU till SP
				//  (this may set earlier SP)
				PRT_DEBUG ("Sched: Scheduling CPU till " << time);
				cpu->executeUntilTarget(time);
			}
		} else {
			// if CPU has reached SP, emulate the device
			pop_heap(syncPoints.begin(), syncPoints.end());
			syncPoints.pop_back();
			sem.up();
			Schedulable* device = sp.getDevice();
			assert(device);
			int userData = sp.getUserData();
			PRT_DEBUG ("Sched: Scheduling " << device->schedName()
			           << " " << userData << " till " << time);
			device->executeUntil(time, userData);
		}
		if (!powerSetting.getValue()) {
			assert(renderer);
			int fps = renderer->putPowerOffImage();
			if (fps == 0) {
				eventDistributor->wait();
			} else {
				SDL_Delay(1000 / fps);
				eventDistributor->poll();
			}
		} else if (paused) {
			assert(renderer);
			renderer->putStoredImage();
			eventDistributor->wait();
		} else {
			eventDistributor->poll();
		}
	}
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

void Scheduler::powerOn()
{
	powerSetting.setValue(true);
	Leds::instance()->setLed(Leds::POWER_ON);
	RealTime::instance()->resync();
}

void Scheduler::powerOff()
{
	powerSetting.setValue(false);
	Leds::instance()->setLed(Leds::POWER_OFF);
	MSXMotherBoard::instance()->reInitMSX();
}

void Scheduler::update(const SettingLeafNode* setting)
{
	if (setting == &pauseSetting) {
		if (pauseSetting.getValue()) {
			// VDP has taken over this role.
			// TODO: Should it stay that way?
			// pause();
		} else {
			unpause();
		}
	} else if (setting == &powerSetting) {
		if (powerSetting.getValue()) {
			powerOn();
		} else {
			powerOff();
		}
	} else {
		assert(false);
	}
}

} // namespace openmsx

