#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
// in MSXDevice.h : typedef unsigned long int UINT64;

class MSXZ80;

class SchedulerNode
{
	public: 
		SchedulerNode *next;
		MSXDevice *device;
		UINT64 tstamp;

		SchedulerNode(UINT64 time,MSXDevice *dev)
		{
			tstamp=time;
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
		UINT64 currentTState;
		int schedulerFreq;
		int stateIRQline;
		int keepRunning;
	public:
		Scheduler(void);
		~Scheduler(void);
		UINT64 getCurrentTime();
		UINT64 getFirstStamp();
		void removeFirstStamp();
		int getTimeMultiplier(int nativeFreq);
		void insertStamp(UINT64 timestamp,MSXDevice *activedevice);
		void setLaterSP(UINT64 latertimestamp,MSXDevice *activedevice);
		void raiseIRQ();
		void lowerIRQ();
		int getIRQ();
		void scheduleEmulation();
		void stopEmulation();

		static MSXZ80 *nowRunning; //temporary hack for Z80: DO NOT USE
};

#endif
