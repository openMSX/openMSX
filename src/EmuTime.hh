// $Id$

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#include <iostream>
#include <cassert>
#include "openmsx.hh"

using std::ostream;

// predefines
class EmuTime;
ostream &operator <<(ostream &os, const EmuTime &e);

// constants
const uint64 MAIN_FREQ = 3579545 * 24;


class EmuDuration
{
	public:
		// friends
		friend class EmuTime;
	
		// constructors
		EmuDuration()                  { time = 0; }
		explicit EmuDuration(uint64 n) { time = n; }
		explicit EmuDuration(double duration) 
			{ time = (uint64)(duration * MAIN_FREQ); }

		// conversions
		float toFloat() const { return (float)time / MAIN_FREQ; }
		uint64 length() const { return time; }
		unsigned frequency() const { return MAIN_FREQ / time; }

		// assignment operator
		EmuDuration &operator =(const EmuDuration &d)
			{ time = d.time; return *this; }
		
		// comparison operators
		bool operator ==(const EmuDuration &d) const
			{ return time == d.time; }
		bool operator !=(const EmuDuration &d) const
			{ return time != d.time; }
		bool operator < (const EmuDuration &d) const
			{ return time <  d.time; }
		bool operator <=(const EmuDuration &d) const
			{ return time <= d.time; }
		bool operator > (const EmuDuration &d) const
			{ return time >  d.time; }
		bool operator >=(const EmuDuration &d) const
			{ return time >= d.time; }
		
		// arithmetic operators
		const EmuDuration operator %(const EmuDuration &d) const 
			{ return EmuDuration(time % d.time); }
		const EmuDuration operator *(unsigned fact) const
			{ return EmuDuration(time * fact); }
		unsigned operator /(const EmuDuration &d) const
			{ return time / d.time; }
		
		static const EmuDuration zero;
		static const EmuDuration infinity;

	private:
		uint64 time;
};

class EmuTime
{
	public:
		// friends
		friend ostream &operator<<(ostream &os, const EmuTime &et);

		// constructors
		EmuTime()                  { time = 0; }
		explicit EmuTime(uint64 n) { time = n; }
		EmuTime(const EmuTime &e)  { time = e.time; }

		// assignment operator
		EmuTime &operator =(const EmuTime &e)
			{ time = e.time; return *this; }

		// comparison operators
		bool operator ==(const EmuTime &e) const
			{ return time == e.time; }
		bool operator !=(const EmuTime &e) const
			{ return time != e.time; }
		bool operator < (const EmuTime &e) const
			{ return time <  e.time; }
		bool operator <=(const EmuTime &e) const
			{ return time <= e.time; }
		bool operator > (const EmuTime &e) const
			{ return time >  e.time; }
		bool operator >=(const EmuTime &e) const
			{ return time >= e.time; }

		// arithmetic operators
		const EmuTime operator +(const EmuDuration &d) const 
			{ return EmuTime(time + d.time); }
		const EmuTime operator -(const EmuDuration &d) const 
			{ assert(time >= d.time);
			  return EmuTime(time - d.time); }
		EmuTime &operator +=(const EmuDuration &d)
			{ time += d.time; return *this; }
		EmuTime &operator -=(const EmuDuration &d)
			{ assert(time >= d.time);
			  time -= d.time; return *this; }
		const EmuDuration operator -(const EmuTime &e) const
			{ assert(time >= e.time);
			  return EmuDuration(time - e.time); }
		
		// ticks
		unsigned getTicksAt(unsigned freq) const
			{ return time / (MAIN_FREQ / freq); }
		
		static const EmuTime zero;
		static const EmuTime infinity;
		
	//protected:
		uint64 time;
};

template <unsigned freq>
class EmuTimeFreq : public EmuTime
{
	public:
		// constructor
		EmuTimeFreq()                          { time  = 0; }
		explicit EmuTimeFreq(const EmuTime &e) { time = e.time; }
		//EmuTimeFreq(uint64 n)   { time  = n * (MAIN_FREQ / freq); }

		void operator()(uint64 n) { time  = n * (MAIN_FREQ / freq); }

		// assignment operator
		EmuTime &operator =(const EmuTime &e)
			{ time = e.time; return *this; }

		// arithmetic operators
		EmuTime &operator +=(unsigned n)
			{ time += n * (MAIN_FREQ / freq); return *this; }
		EmuTime &operator -=(unsigned n)
			{ time -= n * (MAIN_FREQ / freq); return *this; }
		EmuTime &operator ++()
			{ time +=     (MAIN_FREQ / freq); return *this; }
		EmuTime &operator --() 
			{ time -=     (MAIN_FREQ / freq); return *this; }

		const EmuTime operator +(uint64 n) const
			{ return EmuTime(time + n * (MAIN_FREQ / freq)); }

		// distance function
		unsigned getTicksTill(const EmuTime &e) const { 
			assert(e.time >= time); 
			return (e.time - time) / (MAIN_FREQ / freq);
		}
		unsigned getTicksTillUp(const EmuTime &e) const { 
			assert(e.time >= time);
			return (e.time - time + MAIN_FREQ / freq - 1) /
			       (MAIN_FREQ / freq); // round up
		}
};

#endif
