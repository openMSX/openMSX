// $Id$

#ifndef EMUDUARTION_HH
#define EMUDUARTION_HH

#include "openmsx.hh"

namespace openmsx {

// constants
const uint64 MAIN_FREQ = 3579545ULL * 1200ULL;


class EmuDuration
{
public:
	// friends
	friend class EmuTime;

	// constructors
	EmuDuration()                  : time(0) {}
	explicit EmuDuration(uint64 n) : time(n) {}
	explicit EmuDuration(double duration)
		: time((uint64)(duration * MAIN_FREQ)) {}

	// conversions
	double toDouble() const { return (double)time / MAIN_FREQ; }
	uint64 length() const { return time; }
	unsigned frequency() const { return MAIN_FREQ / time; }

	// assignment operator
	EmuDuration& operator=(const EmuDuration& d)
		{ time = d.time; return *this; }

	// comparison operators
	bool operator==(const EmuDuration& d) const
		{ return time == d.time; }
	bool operator!=(const EmuDuration& d) const
		{ return time != d.time; }
	bool operator< (const EmuDuration& d) const
		{ return time <  d.time; }
	bool operator<=(const EmuDuration& d) const
		{ return time <= d.time; }
	bool operator> (const EmuDuration& d) const
		{ return time >  d.time; }
	bool operator>=(const EmuDuration& d) const
		{ return time >= d.time; }

	// arithmetic operators
	const EmuDuration operator%(const EmuDuration& d) const
		{ return EmuDuration(time % d.time); }
	const EmuDuration operator+(const EmuDuration& d) const
		{ return EmuDuration(time + d.time); }
	const EmuDuration operator*(unsigned fact) const
		{ return EmuDuration(time * fact); }
	const EmuDuration operator/(unsigned fact) const
		{ return EmuDuration(time / fact); }
	unsigned operator/(const EmuDuration& d) const
		{ return time / d.time; }

	EmuDuration& operator*=(double fact)
		{ time = static_cast<uint64>(time * fact); return *this; }
	EmuDuration& operator/=(double fact)
		{ time = static_cast<uint64>(time / fact); return *this; }

	// ticks
	// TODO: Used in WavAudioInput. Keep or use DynamicClock instead?
	unsigned getTicksAt(unsigned freq) const
		{ return time / (MAIN_FREQ / freq); }

	static const EmuDuration zero;
	static const EmuDuration infinity;

private:
	uint64 time;
};

} // namespace openmsx

#endif
