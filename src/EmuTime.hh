// $Id$

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#include <iostream>
#include <cassert>
#include "openmsx.hh"

// predefines
class EmuTime;
std::ostream &operator<<(std::ostream &os, const EmuTime &e);

// constants
const uint64 MAIN_FREQ = 3579545*24;


class EmuDuration
{
	public:
		// friends
		friend class EmuTime;
	
		// constructors
		EmuDuration(uint64 n)        { time = n; }
		EmuDuration(double duration) { time = (uint64)(duration*MAIN_FREQ); }

		// conversions
		float toFloat() const { return (float)time / MAIN_FREQ; }
		uint64 length() const { return time; }

	private:
		uint64 time;
};

class EmuTime
{
	public:
		// friends
		friend std::ostream &operator<<(std::ostream &os, const EmuTime &et);

		// constants
		// Do not use INFINITY because it is macro-expanded on some systems
		static const uint64 INFTY = 18446744073709551615ULL; //ULLONG_MAX;

		// constructors
		EmuTime()                 { time = 0; }
		EmuTime(uint64 n)         { time = n; }
		EmuTime(const EmuTime &e) { time = e.time; }

		// assignment operator
		EmuTime &operator =(const EmuTime &e) { time = e.time; return *this; }

		// comparison operators
		bool operator ==(const EmuTime &e) const { return time == e.time; }
		bool operator < (const EmuTime &e) const { return time <  e.time; }
		bool operator <=(const EmuTime &e) const { return time <= e.time; }
		bool operator > (const EmuTime &e) const { return time >  e.time; }
		bool operator >=(const EmuTime &e) const { return time >= e.time; }

		// arithmetic operators
		EmuTime operator +(const EmuDuration &d) const 
			{ return EmuTime(time + d.time); }
		EmuTime operator -(const EmuDuration &d) const 
			{ assert(time >= d.time);
			  return EmuTime(time - d.time); }
		EmuTime &operator +=(const EmuDuration &d)
			{ time += d.time; return *this; }
		EmuTime &operator -=(const EmuDuration &d)
			{ assert(time >= d.time);
			  time -= d.time; return *this; }
		EmuDuration operator -(const EmuTime &e) const
			{ assert(time >= e.time);
			  return EmuDuration(time-e.time); }
		
		static const EmuTime zero;
		static const EmuTime infinity;
		
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

		EmuTime operator +(uint64 n) { return EmuTime(time+n*(MAIN_FREQ/freq)); }

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
