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
	//	removeFirstStamp();
	//}
}

const Emutime &Scheduler::getFirstStamp()
{
	return scheduleList.getFirst().getTime();
}

void Scheduler::removeFirstStamp()
{
	scheduleList.removeFirst();
}


const Emutime &Scheduler::getCurrentTime()
{
	return currentTime;
}

void Scheduler::insertStamp(Emutime &timestamp, MSXDevice &activedevice) 
{
	assert (timestamp >= getCurrentTime());
	scheduleList.insert (SchedulerNode (timestamp, activedevice));
}

//void Scheduler::setLaterSP(UINT64 latertimestamp,MSXDevice *activedevice)
//{
//	insertStamp(getCurrentTime()+latertimestamp , activedevice);
//}

void Scheduler::raiseIRQ()
{
	stateIRQline++;
}

void Scheduler::lowerIRQ()
{
	assert (stateIRQline != 0);
	stateIRQline--;
}

int Scheduler::getIRQ()
{
	return stateIRQline;
}
void Scheduler::scheduleEmulation()
{
	keepRunning=true;
	while(keepRunning){
		const SchedulerNode &firstNode = scheduleList.getFirst();
		const Emutime &time = firstNode.getTime();
		MSXDevice device = firstNode.getDevice();
	//1. Set the target T-State of the cpu to the first SP in the list.
	// and let the CPU execute until the target T-state.
		nowRunning=MSXMotherBoard::CPU;
		MSXMotherBoard::CPU->executeUntilEmuTime(time);
	// Time is now updated
		currentTime=time;
	//3. Get the device from the first SP in the list and let it reach its T-state.
		device.executeUntilEmuTime(time);
	//4. Remove the first element from the list
		removeFirstStamp();
	// TODO: loop if there are other devices with the same timestamp
	}
}

void Scheduler::stopEmulation()
{
	keepRunning=false;
}

