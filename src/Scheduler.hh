// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include "emutime.hh"
#include "MSXDevice.hh"
#include <set>


class SynchronizationPoint
{
	public:
		SynchronizationPoint (const Emutime &time, MSXDevice &dev) : timeStamp(time), device(dev) {}
		const Emutime &getTime() const { return timeStamp; }
		MSXDevice &getDevice() const { return device; }
		bool operator< (const SynchronizationPoint &n) const { return getTime() < n.getTime(); }
	private: 
		const Emutime timeStamp;	// copy of original timestamp
		MSXDevice &device;		// alias
};

class Scheduler
{
	public:
		~Scheduler();
		static Scheduler *instance();
		void setSyncPoint(Emutime &timestamp, MSXDevice &activedevice);
		void scheduleEmulation();
	
	private:
		Scheduler();
		const SynchronizationPoint &getFirstSP();
		void removeFirstSP();
		
		static Scheduler *oneInstance;
		std::set<SynchronizationPoint> scheduleList;

		static const Emutime infinity;
};

#endif
