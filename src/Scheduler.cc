// $Id$

#include <cassert>
#include <algorithm>
#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "EventDistributor.hh"
#include "Schedulable.hh"
#include "CommandController.hh"
#include "Leds.hh"
#include "Renderer.hh" // TODO: Temporary?

using std::swap;

namespace openmsx {

const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
	: sem(1),
	  pauseSetting("pause", "pauses the emulation", false),
	  powerSetting("power", "turn power on/off", false),
	  leds(Leds::instance()),
	  cpu(MSXCPU::instance()),
	  commandController(CommandController::instance()),
	  eventDistributor(EventDistributor::instance()),
	  quitCommand(*this)
{
	paused = false;
	emulationRunning = true;
	cpu.init(this);
	renderer = NULL;

	pauseSetting.addListener(this);
	powerSetting.addListener(this);

	commandController.registerCommand(&quitCommand, "quit");
	eventDistributor.registerEventListener(SDL_QUIT, this);
}

Scheduler::~Scheduler()
{
	eventDistributor.unregisterEventListener(SDL_QUIT, this);
	commandController.unregisterCommand(&quitCommand, "quit");

	powerSetting.removeListener(this);
	pauseSetting.removeListener(this);
}

Scheduler& Scheduler::instance()
{
	static Scheduler oneInstance;
	return oneInstance;
}

void Scheduler::setSyncPoint(const EmuTime &timeStamp, Schedulable *device, int userData)
{
	if (device) {
		//PRT_DEBUG("Sched: registering " << device->schedName() <<
		//          " " << userData << " for emulation at " << time);
	}

	sem.down();
	// Push sync point into queue.
	syncPoints.push_back(SynchronizationPoint(timeStamp, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
	// Tell CPU emulation to return early if necessary.
	// TODO: Emulation may run in parallel in a seperate thread.
	//       What we're doing here is not thread safe.
	if (timeStamp < cpu.getTargetTime()) {
		cpu.setTargetTime(timeStamp);
	}
	sem.up();

	if (paused || !powerSetting.getValue()) {
		eventDistributor.notify();
	}
}

void Scheduler::removeSyncPoint(Schedulable* device, int userData)
{
	sem.down();
	for (vector<SynchronizationPoint>::iterator it = syncPoints.begin();
	     it != syncPoints.end(); ++it) {
		SynchronizationPoint& sp = *it;
		if ((sp.getDevice() == device) &&
		    (sp.getUserData() == userData)) {
			swap(sp, syncPoints.back());
			syncPoints.pop_back();
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

void Scheduler::schedule(const EmuTime &limit)
{
	while (true) {
		// Get next sync point.
		sem.down();
		const SynchronizationPoint sp =
			  syncPoints.empty()
			? SynchronizationPoint(EmuTime::infinity, NULL, 0)
			: syncPoints.front();
		const EmuTime &time = sp.getTime();

		// Return when we've gone far enough.
		// If limit and time are both infinity, scheduling will continue.
		if (limit < time || !emulationRunning) {
			sem.up();
			return;
		}

		if (time == ASAP) {
			scheduleDevice(sp, cpu.getCurrentTime());
			eventDistributor.poll();
		} else if (!powerSetting.getValue()) {
			sem.up();
			assert(renderer);
			int fps = renderer->putPowerOffImage();
			if (fps == 0) {
				eventDistributor.wait();
			} else {
				SDL_Delay(1000 / fps);
				eventDistributor.poll();
			}
		} else if (paused) {
			sem.up();
			assert(renderer);
			renderer->putStoredImage();
			eventDistributor.wait();
		} else {
			if (cpu.getTargetTime() < time) {
				sem.up();
				// Schedule CPU until first sync point.
				// This may set earlier sync point.
				PRT_DEBUG ("Sched: Scheduling CPU till " << time);
				cpu.executeUntilTarget(time);
			} else {
				scheduleDevice(sp, time);
			}
			eventDistributor.poll();
		}
	}
}

void Scheduler::scheduleDevice(
	const SynchronizationPoint &sp, const EmuTime &time )
{
	pop_heap(syncPoints.begin(), syncPoints.end());
	syncPoints.pop_back();
	sem.up();
	Schedulable *device = sp.getDevice();
	assert(device);
	int userData = sp.getUserData();
	PRT_DEBUG ("Sched: Scheduling " << device->schedName()
			<< " " << userData << " till " << time);
	device->executeUntil(time, userData);
}

void Scheduler::unpause()
{
	paused = false;
}

void Scheduler::pause()
{
	paused = true;
}

void Scheduler::powerOn()
{
	powerSetting.setValue(true);
	leds.setLed(Leds::POWER_ON);
}

void Scheduler::powerOff()
{
	powerSetting.setValue(false);
	leds.setLed(Leds::POWER_OFF);
}

void Scheduler::update(const SettingLeafNode* setting) throw()
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

bool Scheduler::signalEvent(const SDL_Event& event) throw()
{
	stopScheduling();
	return true;
}


// class QuitCommand

Scheduler::QuitCommand::QuitCommand(Scheduler& parent_)
	: parent(parent_)
{
}

string Scheduler::QuitCommand::execute(const vector<string> &tokens)
	throw()
{
	parent.stopScheduling();
	return "";
}

string Scheduler::QuitCommand::help(const vector<string> &tokens) const
	throw()
{
	return "Use this command to stop the emulator\n";
}

} // namespace openmsx

