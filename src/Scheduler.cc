// $Id$

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include <cassert>


Scheduler::Scheduler()
{
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


void Scheduler::setSyncPoint(Emutime &time, MSXDevice &device) 
{
	assert (time >= MSXCPU::instance()->getCurrentTime());
	if (time < MSXCPU::instance()->getTargetTime())
		MSXCPU::instance()->setTargetTime(time);
	scheduleList.insert(SynchronizationPoint (time, device));
}

const SynchronizationPoint &Scheduler::getFirstSP()
{
	assert (!scheduleList.empty());
	return *(scheduleList.begin());
}

void Scheduler::removeFirstSP()
{
	assert (!scheduleList.empty());
	scheduleList.erase(scheduleList.begin());
}


void Scheduler::scheduleEmulation()
{
	while (true) {

// some test stuff for joost, please leave for a few
//	std::cerr << "foo" << std::endl;
//	std::ifstream quakef("starquake/quake");
//	byte quakeb[128];
//	quakef.read(quakeb, 127);
//	Emutime foo;
//	std::cerr << "foo" << std::endl;
//	for (int joosti=0;joosti<127;joosti++)
//	{
//		MSXMotherBoard::instance()->writeMem(0x8000+joosti,quakeb[joosti],foo);
//	}
//	quakef.close();

		if (scheduleList.empty()) {
			// nothing scheduled, emulate CPU
			PRT_DEBUG ("Scheduling CPU till infinity");
			MSXCPU::instance()->executeUntilTarget(infinity);
		} else {
			const SynchronizationPoint &sp = getFirstSP();
			const Emutime &time = sp.getTime();
			if (MSXCPU::instance()->getCurrentTime() < time) {
				// emulate CPU till first SP, don't immediately emulate
				// device since CPU could not have reached SP
				PRT_DEBUG ("Scheduling CPU till SP");
				MSXCPU::instance()->executeUntilTarget(time);
			} else {
				// if CPU has reached SP, emulate the device
				MSXDevice *device = &(sp.getDevice());
				PRT_DEBUG ("Scheduling " << device->getName());
				device->executeUntilEmuTime(time);
				removeFirstSP();
			}
		}
	}
}
const Emutime Scheduler::infinity = Emutime(1, Emutime::INFINITY);
