#ifndef EMUTIME_HH
#define EMUTIME_HH

#include "EmuDuration.hh"

#include "serialize.hh"

#include <iosfwd>
#include <cassert>
#include <limits>

namespace openmsx {

class EmuTime
{
public:
	static EmuTime dummy() { return {}; }

	// Note: default copy constructor and assignment operator are ok.

	static constexpr EmuTime makeEmuTime(uint64_t u) { return EmuTime(u); }

	// comparison operators
	[[nodiscard]] constexpr auto operator<=>(const EmuTime&) const = default;

	// arithmetic operators
	// TODO these 2 should be 'friend', workaround for pre-gcc-13 bug
	[[nodiscard]] constexpr EmuTime operator+(const EmuDuration& r) const
		{ return EmuTime(time + r.time); }
	[[nodiscard]] constexpr EmuTime operator-(const EmuDuration& r) const
		{ assert(time >= r.time);
		  return EmuTime(time - r.time); }
	constexpr EmuTime& operator+=(EmuDuration d)
		{ time += d.time; return *this; }
	constexpr EmuTime& operator-=(EmuDuration d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	[[nodiscard]] constexpr friend EmuDuration operator-(const EmuTime& l, const EmuTime& r)
		{ assert(l.time >= r.time);
		  return EmuDuration(l.time - r.time); }

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
	friend std::ostream& operator<<(std::ostream& os, EmuTime time);
	friend class DynamicClock;
	template<unsigned, unsigned> friend class Clock;
};

template<> struct SerializeAsMemcpy<EmuTime> : std::true_type {};

} // namespace openmsx

#endif
