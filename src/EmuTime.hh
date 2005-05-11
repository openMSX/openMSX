// $Id$

#ifndef EMUTIME_HH
#define EMUTIME_HH

#include "EmuDuration.hh"
#include <iosfwd>
#include <cassert>

namespace openmsx {

class EmuTime
{
public:
	// friends
	friend std::ostream& operator<<(std::ostream& os, const EmuTime& time);
	template<unsigned> friend class Clock;
	friend class DynamicClock;

	// constructors
	EmuTime()                  : time(0) {}
	explicit EmuTime(uint64 n) : time(n) {}
	EmuTime(const EmuTime& e)  : time(e.time) {}

	// destructor
	virtual ~EmuTime() {}

	// assignment operator
	EmuTime& operator=(const EmuTime& e)
		{ time = e.time; return *this; }

	// comparison operators
	bool operator==(const EmuTime& e) const
		{ return time == e.time; }
	bool operator!=(const EmuTime& e) const
		{ return time != e.time; }
	bool operator< (const EmuTime& e) const
		{ return time <  e.time; }
	bool operator<=(const EmuTime& e) const
		{ return time <= e.time; }
	bool operator> (const EmuTime& e) const
		{ return time >  e.time; }
	bool operator>=(const EmuTime& e) const
		{ return time >= e.time; }

	// arithmetic operators
	const EmuTime operator+(const EmuDuration& d) const
		{ return EmuTime(time + d.time); }
	const EmuTime operator-(const EmuDuration& d) const
		{ assert(time >= d.time);
		  return EmuTime(time - d.time); }
	EmuTime& operator+=(const EmuDuration& d)
		{ time += d.time; return *this; }
	EmuTime& operator-=(const EmuDuration& d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	const EmuDuration operator-(const EmuTime& e) const
		{ assert(time >= e.time);
		  return EmuDuration(time - e.time); }

	static const EmuTime zero;
	static const EmuTime infinity;

private:
	uint64 time;
};

std::ostream& operator <<(std::ostream& os, const EmuTime& e);

} // namespace openmsx

#endif
