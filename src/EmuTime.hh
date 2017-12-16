#ifndef EMUTIME_HH
#define EMUTIME_HH

#include "EmuDuration.hh"
#include <iosfwd>
#include <cassert>

namespace openmsx {

// EmuTime is only a very small class (one 64-bit member). On 64-bit CPUs
// it's cheaper to pass this by value. On 32-bit CPUs pass-by-reference
// is cheaper.
template <typename T, bool C = sizeof(void*) < 8> struct EmuTime_param_impl;
template <typename T> struct EmuTime_param_impl<T, true> { // pass by reference
	using param = const T&;
	static param dummy() { return T::zero; }
};
template <typename T> struct EmuTime_param_impl<T, false> { // pass by value
	using param = const T;
	static param dummy() { return T(); }
};

class EmuTime
{
public:
	using param = EmuTime_param_impl<EmuTime>::param;
	static param dummy() { return EmuTime_param_impl<EmuTime>::dummy(); }

	// Note: default copy constructor and assigment operator are ok.

	static EmuTime makeEmuTime(uint64_t u) { return EmuTime(u); }

	// comparison operators
	bool operator==(EmuTime::param e) const
		{ return time == e.time; }
	bool operator!=(EmuTime::param e) const
		{ return time != e.time; }
	bool operator< (EmuTime::param e) const
		{ return time <  e.time; }
	bool operator<=(EmuTime::param e) const
		{ return time <= e.time; }
	bool operator> (EmuTime::param e) const
		{ return time >  e.time; }
	bool operator>=(EmuTime::param e) const
		{ return time >= e.time; }

	// arithmetic operators
	const EmuTime operator+(EmuDuration::param d) const
		{ return EmuTime(time + d.time); }
	const EmuTime operator-(EmuDuration::param d) const
		{ assert(time >= d.time);
		  return EmuTime(time - d.time); }
	EmuTime& operator+=(EmuDuration::param d)
		{ time += d.time; return *this; }
	EmuTime& operator-=(EmuDuration::param d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	const EmuDuration operator-(EmuTime::param e) const
		{ assert(time >= e.time);
		  return EmuDuration(time - e.time); }

	static const EmuTime zero;
	static const EmuTime infinity;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	EmuTime() {} // uninitialized
	explicit EmuTime(uint64_t n) : time(n) {}

	uint64_t time;

	// friends
	friend EmuTime_param_impl<EmuTime>;
	friend std::ostream& operator<<(std::ostream& os, EmuTime::param time);
	template<unsigned, unsigned> friend class Clock;
	friend class DynamicClock;
};

std::ostream& operator <<(std::ostream& os, EmuTime::param e);

} // namespace openmsx

#endif
