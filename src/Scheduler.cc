// $Id$

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include <assert.h>


MSXZ80 *Scheduler::nowRunning; //temporary hack for Z80: DO NOT USE
// Read timescheduler.txt for a more detailed explication of the ideas behind the scheduler class

Scheduler::Scheduler(void) : currentTime()
{
}

Scheduler::~Scheduler(void)
{
	//while (!scheduleList.empty()) {
	//	removeFirstNode();
	//}
}

Scheduler* Scheduler::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new Scheduler();
	}
	return oneInstance;
}

Scheduler *Scheduler::oneInstance;

const SchedulerNode &Scheduler::getFirstNode()
{
	assert (!scheduleList.empty());
	return *(scheduleList.begin());
}

void Scheduler::removeFirstNode()
{
	assert (!scheduleList.empty());
	scheduleList.erase(scheduleList.begin());
}


const Emutime &Scheduler::getCurrentTime()
{
	return currentTime;
}

void Scheduler::insertStamp(Emutime &timestamp, MSXDevice &activedevice) 
{
	assert (timestamp >= getCurrentTime());
	scheduleList.insert(SchedulerNode (timestamp, activedevice));
}

void Scheduler::scheduleEmulation()
{
	keepRunning=true;
	while(keepRunning){
		//const SchedulerNode &firstNode = scheduleList.getFirst();
		const SchedulerNode &firstNode = getFirstNode();
		const Emutime &time = firstNode.getTime();
		MSXDevice *device = &firstNode.getDevice();
	//1. Set the target T-State of the cpu to the first SP in the list.
	// and let the CPU execute until the target T-state.
		PRT_DEBUG ("Scheduling CPU\n");
		nowRunning=MSXMotherBoard::CPU;
		MSXMotherBoard::CPU->executeUntilEmuTime(time);
	// Time is now updated
		currentTime=time;
	//3. Get the device from the first SP in the list and let it reach its T-state.
		// TODO following print gives seg fault
		PRT_DEBUG ("Scheduling " << device->getName() << "\n");
		device->executeUntilEmuTime(time);
	//4. Remove the first element from the list
		removeFirstNode();
	// TODO: loop if there are other devices with the same timestamp
	}
}

void Scheduler::stopEmulation()
{
	keepRunning=false;
}


