// $Id$

#ifndef __Y8950TIMER_HH__
#define __Y8950TIMER_HH__

#include "Schedulable.hh"
#include "openmsx.hh"

// forward declarations
class Y8950;
class Scheduler;


template<int freq, byte flag>
class Y8950Timer : Schedulable
{
	public:
		Y8950Timer(Y8950 *y8950);
		virtual ~Y8950Timer();
		void setValue(byte value);
		void setStart(bool start, const EmuTime &time);

	private:
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		void schedule(const EmuTime &time);
		void unschedule();
	
		int count;
		bool counting;
		Y8950 *y8950;
		Scheduler *scheduler;
};

#endif
