// $Id$

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include "assert.h"


MSXZ80 *Scheduler::nowRunning; //temporary hack for Z80: DO NOT USE
// Read timescheduler.txt for a more detailed explication of the ideas behind the scheduler class

Scheduler::Scheduler(void) : currentTime()
{
	Start=End=Current=0;
}

Scheduler::~Scheduler(void)
{
	while (Start != 0){ 
		tmp=Start;
		Start=Start->next;
		delete tmp;
	}
}

Emutime &Scheduler::getFirstStamp()
{
	assert (Start != 0);
	return Start->tstamp;
}

void Scheduler::removeFirstStamp()
{
	assert (Start != 0);
	tmp=Start;
	Start=Start->next;
	delete tmp;
}


Emutime &Scheduler::getCurrentTime()
{
	return currentTime;
}

void Scheduler::insertStamp(Emutime &timestamp, MSXDevice *activedevice) 
{
	assert (timestamp >= getCurrentTime());
	tmp=new SchedulerNode(timestamp,activedevice);
	// If there is no list
	if (Start == NULL){
		Start=tmp;
		End=tmp;
		return;
	}
	// Insert before start if smaller or equal
	if (timestamp <= Start->tstamp){
		tmp->next=Start;
		Start=tmp;
		return;
	}
	//Insert after end if greater or equal
	if (timestamp >= End->tstamp){
		End->next=tmp;
		End=tmp;
		return;
	}
	// You only get here if start<tmp<end
	Current=Start;
	while ( (timestamp > Current->next->tstamp ) && (Current->next != End) ){
		Current=Current->next;
	}
	tmp->next=Current->next;
	Current->next=tmp;
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
	keepRunning=1;
	while(keepRunning){
	//1. Set the target T-State of the cpu to the first SP in the list.
	// and let the CPU execute until the target T-state.
		nowRunning=MSXMotherBoard::CPU;
		MSXMotherBoard::CPU->executeUntilEmuTime(Start->tstamp);
	// Time is now updated
		currentTime=Start->tstamp;
	//3. Get the device from the first SP in the list and let it reach its T-state.
		Start->device->executeUntilEmuTime(Start->tstamp);
	//4. Remove the first element from the list
		removeFirstStamp();
	// TODO: loop if there are other devices with the same timestamp
	}
}

void Scheduler::stopEmulation()
{
	keepRunning=0;
}

