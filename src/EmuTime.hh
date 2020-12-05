#ifndef EMUTIME_HH
#define EMUTIME_HH

#include "EmuDuration.hh"
#include "serialize.hh"
#include <iosfwd>
#include <cassert>
#include <limits>

namespace openmsx {

// EmuTime is only a very small class (one 64-bit member). On 64-bit CPUs
// it's cheaper to pass this by value. On 32-bit CPUs pass-by-reference
// is cheaper.
template<typename T, bool C = sizeof(void*) < 8> struct EmuTime_param_impl;
template<typename T> struct EmuTime_param_impl<T, true> { // pass by reference
	using param = const T&;
	static param dummy() { static constexpr auto e = T::zero(); return e; }
};
template<typename T> struct EmuTime_param_impl<T, false> { // pass by value
	using param = T;
	static param dummy() { return T(); }
};

class EmuTime
{
public:
	using param = EmuTime_param_impl<EmuTime>::param;
	static param dummy() { return EmuTime_param_impl<EmuTime>::dummy(); }

	// Note: default copy constructor and assignment operator are ok.

	static constexpr EmuTime makeEmuTime(uint64_t u) { return EmuTime(u); }

	// comparison operators
	[[nodiscard]] constexpr bool operator==(EmuTime::param e) const
		{ return time == e.time; }
	[[nodiscard]] constexpr bool operator!=(EmuTime::param e) const
		{ return time != e.time; }
	[[nodiscard]] constexpr bool operator< (EmuTime::param e) const
		{ return time <  e.time; }
	[[nodiscard]] constexpr bool operator<=(EmuTime::param e) const
		{ return time <= e.time; }
	[[nodiscard]] constexpr bool operator> (EmuTime::param e) const
		{ return time >  e.time; }
	[[nodiscard]] constexpr bool operator>=(EmuTime::param e) const
		{ return time >= e.time; }

	// arithmetic operators
	[[nodiscard]] constexpr EmuTime operator+(EmuDuration::param d) const
		{ return EmuTime(time + d.time); }
	[[nodiscard]] constexpr EmuTime operator-(EmuDuration::param d) const
		{ assert(time >= d.time);
		  return EmuTime(time - d.time); }
	constexpr EmuTime& operator+=(EmuDuration::param d)
		{ time += d.time; return *this; }
	constexpr EmuTime& operator-=(EmuDuration::param d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	[[nodiscard]] constexpr EmuDuration operator-(EmuTime::param e) const
		{ assert(time >= e.time);
		  return EmuDuration(time - e.time); }

	[[nodiscard]] static constexpr EmuTime zero()
	{
		return EmuTime(uint64_t(0));
	}
	[[nodiscard]] static constexpr EmuTime infinity()
	{
		return EmuTime(std::numeric_limits<uint64_t>::max());
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

private:
	EmuTime() = default; // uninitialized
	constexpr explicit EmuTime(uint64_t n) : time(n) {}

private:
	uint64_t time;

	// friends
	friend EmuTime_param_impl<EmuTime>;
	friend std::ostream& operator<<(std::ostream& os, EmuTime::param time);
	friend class DynamicClock;
	template<unsigned, unsigned> friend class Clock;
};

template<> struct SerializeAsMemcpy<EmuTime> : std::true_type {};

} // namespace openmsx

#endif
