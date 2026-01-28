#ifndef EMUTIME_HH
#define EMUTIME_HH

#include "EmuDuration.hh"

#include "serialize.hh"

#include <cassert>
#include <iosfwd>
#include <limits>

namespace openmsx {

class EmuTime
{
public:
	static EmuTime dummy() { return {}; }

	// Note: default copy constructor and assignment operator are ok.

	[[nodiscard]] static constexpr EmuTime fromDouble(double d) {
		return EmuTime::zero() + EmuDuration::sec(d);
	}

	[[nodiscard]] constexpr uint64_t toUint64() const { return time; }
	[[nodiscard]] constexpr double toDouble() const { return EmuDuration(time).toDouble(); }

	// comparison operators
	[[nodiscard]] constexpr auto operator<=>(const EmuTime&) const = default;

	// arithmetic operators
	[[nodiscard]] constexpr friend EmuTime operator+(EmuTime l, EmuDuration r)
		{ return EmuTime(l.toUint64() + r.toUint64()); }
	[[nodiscard]] constexpr friend EmuTime operator-(EmuTime l, EmuDuration r)
		{ assert(l.toUint64() >= r.toUint64());
		  return EmuTime(l.toUint64() - r.toUint64()); }
	[[nodiscard]] constexpr EmuTime saturateSubtract(EmuDuration r) const
		{ return EmuTime((time >= r.time) ? time - r.time : 0); }
	constexpr EmuTime& operator+=(EmuDuration d)
		{ time += d.time; return *this; }
	constexpr EmuTime& operator-=(EmuDuration d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	[[nodiscard]] constexpr friend EmuDuration operator-(EmuTime l, EmuTime r)
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
