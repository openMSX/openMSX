// $Id$

#include "Scheduler.hh"
#include "MSXCPU.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "Mixer.hh"
#include <cassert>
#include <algorithm>
#include "EventDistributor.hh"
#include "Schedulable.hh"


const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
{
	paused = false;
	noSound = false;
	needBlock = false;
	exitScheduler = false;
	cpu = MSXCPU::instance();
	
	EventDistributor::instance()->registerEventListener(SDL_QUIT, this);
	CommandController::instance()->registerCommand(quitCmd, "quit");
	CommandController::instance()->registerCommand(muteCmd, "mute");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F12, "quit");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F11, "mute");
}

Scheduler::~Scheduler()
{
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F12, "quit");
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F11, "mute");
	CommandController::instance()->unregisterCommand("quit");
	CommandController::instance()->unregisterCommand("mute");
	EventDistributor::instance()->unregisterEventListener(SDL_QUIT, this);
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
	
	EmuTime time = (timeStamp == ASAP) ? targetTime : timeStamp;
	
	if (device) { 
		PRT_DEBUG("Sched: registering " << device->schedName() << 
		          " " << userData << " for emulation at " << time);
	}

	if (time < targetTime) {
		targetTime = time;
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
	exitScheduler = true;
	setSyncPoint(ASAP, NULL);
	unpause();
}


void Scheduler::scheduleEmulation()
{
	while (!exitScheduler) {
		schedMutex.grab();
		if (syncPoints.empty()) {
			// nothing scheduled, emulate CPU
			schedMutex.release();
			if (!paused) {
				PRT_DEBUG ("Sched: Scheduling CPU till infinity");
				const EmuTime infinity = EmuTime(EmuTime::INFTY);
				targetTime = infinity; // TODO
				cpu->executeUntilTarget(infinity);
			} else {
				needBlock = true;
			}
		} else {
			const SynchronizationPoint sp = *(syncPoints.begin());
			const EmuTime &time = sp.getTime();
			if (targetTime < time) {
				schedMutex.release();
				// first bring CPU till SP 
				//  (this may set earlier SP)
				if (!paused) {
					PRT_DEBUG ("Sched: Scheduling CPU till " << time);
					targetTime = time;
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
		}
		if (needBlock) {
			pauseCond.wait();
		}
	}
}

void Scheduler::unpause()
{
	if (paused) {
		paused = false;
		needBlock = false;
		Mixer::instance()->pause(noSound);
		PRT_DEBUG("Unpaused");
		pauseCond.signal();
	}
}
void Scheduler::pause()
{
	if (!paused) {
		paused = true;
		Mixer::instance()->pause(true);
		PRT_DEBUG("Paused");
	}
}
bool Scheduler::isPaused()
{
	return paused;
}


bool Scheduler::signalEvent(SDL_Event &event)
{
	stopScheduling();
	return true;
}


void Scheduler::QuitCmd::execute(const std::vector<std::string> &tokens)
{
	Scheduler::instance()->stopScheduling();
}
void Scheduler::QuitCmd::help(const std::vector<std::string> &tokens) const
{
	print("Use this command to stop the emulator");
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
			// fall through
		default:
			throw CommandException("Syntax error");
	}
	Mixer::instance()->pause(sch->noSound||sch->isPaused());
}
void Scheduler::MuteCmd::help(const std::vector<std::string> &tokens) const
{
	print("Use this command to mute/unmute the emulator");
	print(" mute:     toggle mute");
	print(" mute on:  set muted");
	print(" mute off: set unmuted");
}

