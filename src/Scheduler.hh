// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__
// in MSXDevice.h : typedef unsigned long int UINT64;

#include "emutime.hh"

class MSXZ80;

class SchedulerNode
{
	public: 
		SchedulerNode *next;
		MSXDevice *device;
		Emutime tstamp;

		SchedulerNode(Emutime &time, MSXDevice *dev) : tstamp(time)
		{
			device=dev;
			next=0;
		};
};

class Scheduler
{
	private:
		SchedulerNode *Start;
		SchedulerNode *End;
		SchedulerNode *Current;
		SchedulerNode *tmp;
		Emutime currentTime;
		int schedulerFreq;
		int stateIRQline;
		int keepRunning;
	public:
		Scheduler(void);
		~Scheduler(void);
		Emutime &getCurrentTime();
		Emutime &getFirstStamp();
		void removeFirstStamp();
		int getTimeMultiplier(int nativeFreq);
		void insertStamp(Emutime &timestamp, MSXDevice *activedevice);
		//void setLaterSP(Emutime &latertimestamp, MSXDevice *activedevice);
		void raiseIRQ();
		void lowerIRQ();
		int getIRQ();
		void scheduleEmulation();
		void stopEmulation();

		static MSXZ80 *nowRunning; //temporary hack for Z80: DO NOT USE
};

#endif
