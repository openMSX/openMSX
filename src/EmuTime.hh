// $Id$

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#include <iostream>
#include <cassert>
#include "openmsx.hh"

// predefines
class EmuTime;
std::ostream &operator<<(std::ostream &os, const EmuTime &e);


class EmuTime
{
	public:
		// friends
		friend std::ostream &operator<<(std::ostream &os, const EmuTime &et);

		// constants
		static const uint64 MAIN_FREQ = 3579545*24;
		static const uint64 INFINITY = 18446744073709551615ULL; //ULLONG_MAX;

		// constructors
		EmuTime()                 { time = 0; }
		EmuTime(uint64 n)         { time = n; }
		EmuTime(const EmuTime &e) { time = e.time; }

		static EmuTime &durationToEmuTime(float duration)
			{ return *new EmuTime((uint64)(duration*MAIN_FREQ)); }

		// assignment operator
		EmuTime &operator =(const EmuTime &e) { time = e.time; return *this; }

		// comparison operators
		bool operator ==(const EmuTime &e) const { return time == e.time; }
		bool operator < (const EmuTime &e) const { return time <  e.time; }
		bool operator <=(const EmuTime &e) const { return time <= e.time; }
		bool operator > (const EmuTime &e) const { return time >  e.time; }
		bool operator >=(const EmuTime &e) const { return time >= e.time; }

		// distance function
		float getDuration(const EmuTime &e) const
			{ return (float)(e.time-time)/MAIN_FREQ; }
		EmuTime &operator -(const EmuTime &e) const
			{ return *new EmuTime(time-e.time); }
		int subtract(const EmuTime &e) const { return (int)(time-e.time); }

	//protected:
		uint64 time;
};

template <int freq>
class EmuTimeFreq : public EmuTime
{
	public:
		// constructor
		EmuTimeFreq()                 { time  = 0; }
		EmuTimeFreq(const EmuTime &e) { time = e.time; }
		//EmuTimeFreq(uint64 n)       { time  = n*(MAIN_FREQ/freq); }

		void operator() (uint64 n)  { time  = n*(MAIN_FREQ/freq); }

		// assignment operator
		EmuTime &operator =(const EmuTime &e) { time = e.time; return *this; }

		// arithmetic operators
		EmuTime &operator +=(int n) { time += n*(MAIN_FREQ/freq); return *this; }
		EmuTime &operator -=(int n) { time -= n*(MAIN_FREQ/freq); return *this; }
		EmuTime &operator ++()      { time +=   (MAIN_FREQ/freq); return *this; } // prefix
		EmuTime &operator --()      { time -=   (MAIN_FREQ/freq); return *this; }
		EmuTime &operator ++(int n) { time +=   (MAIN_FREQ/freq); return *this; } // postfix
		EmuTime &operator --(int n) { time -=   (MAIN_FREQ/freq); return *this; }

		EmuTime &operator +(uint64 n) { return *new EmuTime(time+n*(MAIN_FREQ/freq)); }

		// distance function
		int getTicksTill(const EmuTime &e) const { 
			assert(e.time >= time); 
			return (e.time-time)/(MAIN_FREQ/freq);
		}
		int getTicksTillUp(const EmuTime &e) const { 
			assert(e.time >= time); 
			return (e.time-time+MAIN_FREQ/freq-1)/(MAIN_FREQ/freq); //round up
		}
};

#endif
