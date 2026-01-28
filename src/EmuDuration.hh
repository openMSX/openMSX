#ifndef EMUDURATION_HH
#define EMUDURATION_HH

#include "narrow.hh"
#include "serialize.hh"

#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace openmsx {

// constants
inline constexpr uint64_t MAIN_FREQ = 3579545ULL * 960;
inline constexpr unsigned MAIN_FREQ32 = MAIN_FREQ;
static_assert(MAIN_FREQ < (1ULL << 32), "must fit in 32 bit");
inline constexpr double RECIP_MAIN_FREQ = 1.0 / MAIN_FREQ;


class EmuDuration
{
public:
	// friends
	friend class EmuTime;

	// constructors
	constexpr EmuDuration() = default;
	constexpr explicit EmuDuration(std::unsigned_integral auto n) : time(n) {}

	static constexpr EmuDuration sec(std::integral auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ)); }
	static constexpr EmuDuration sec(std::floating_point auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ + 0.5)); }
	static constexpr EmuDuration msec(std::integral auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1000)); }
	static constexpr EmuDuration msec(std::floating_point auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1e3 + 0.5)); }
	static constexpr EmuDuration usec(std::integral auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1000000)); }
	static constexpr EmuDuration usec(std::floating_point auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1e6 + 0.5)); }
	static constexpr EmuDuration nsec(std::integral auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1000000000)); }
	static constexpr EmuDuration nsec(std::floating_point auto x)
		{ return EmuDuration(uint64_t(x * MAIN_FREQ / 1e9 + 0.5)); }
	static constexpr EmuDuration hz(std::integral auto x)
		{ return EmuDuration(uint64_t(MAIN_FREQ / x)); }
	static constexpr EmuDuration hz(std::floating_point auto x)
		{ return EmuDuration(uint64_t(MAIN_FREQ / x + 0.5)); }

	// conversions
	[[nodiscard]] constexpr double toDouble() const { return double(time) * RECIP_MAIN_FREQ; }
	[[nodiscard]] constexpr uint64_t toUint64() const { return time; }

	// comparison operators
	[[nodiscard]] constexpr auto operator<=>(const EmuDuration&) const = default;

	// arithmetic operators
	[[nodiscard]] constexpr friend EmuDuration operator%(EmuDuration l, EmuDuration r)
		{ return EmuDuration(l.time % r.time); }
	[[nodiscard]] constexpr friend EmuDuration operator+(EmuDuration l, EmuDuration r)
		{ return EmuDuration(l.time + r.time); }
	[[nodiscard]] constexpr friend EmuDuration operator-(EmuDuration l, EmuDuration r)
		{ assert(l.time >= r.time); return EmuDuration(l.time - r.time); }
	[[nodiscard]] constexpr friend EmuDuration operator*(EmuDuration l, std::integral auto fact)
		{ return EmuDuration(l.time * fact); }
	[[nodiscard]] constexpr friend EmuDuration operator*(EmuDuration l, double fact)
		{ return EmuDuration(narrow_cast<uint64_t>(narrow_cast<double>(l.time) * fact)); }
	[[nodiscard]] constexpr friend EmuDuration operator/(EmuDuration l, unsigned fact)
		{ return EmuDuration(l.time / fact); }
	[[nodiscard]] constexpr EmuDuration divRoundUp(unsigned fact) const
		{ return EmuDuration((time + fact - 1) / fact); }
	[[nodiscard]] constexpr friend unsigned operator/(EmuDuration l, EmuDuration r)
	{
		uint64_t result = l.time / r.time;
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	[[nodiscard]] constexpr unsigned divUp(EmuDuration d) const {
		uint64_t result = (time + d.time - 1) / d.time;
#ifdef DEBUG
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	[[nodiscard]] constexpr double div(EmuDuration d) const
		{ return narrow_cast<double>(time) / narrow_cast<double>(d.time); }

	constexpr EmuDuration& operator*=(unsigned fact)
		{ time *= fact; return *this; }
	constexpr EmuDuration& operator*=(double fact)
		{ time = narrow_cast<uint64_t>(narrow_cast<double>(time) * fact); return *this; }
	constexpr EmuDuration& operator/=(double fact)
		{ time = narrow_cast<uint64_t>(narrow_cast<double>(time) / fact); return *this; }

	// The smallest duration larger than zero
	[[nodiscard]] static constexpr EmuDuration epsilon() {
		return EmuDuration(uint64_t(1));
	}

	// ticks
	// TODO: Used in WavAudioInput. Keep or use DynamicClock instead?
	[[nodiscard]] constexpr unsigned getTicksAt(unsigned freq) const
	{
		uint64_t result = time / (MAIN_FREQ32 / freq);
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}

	[[nodiscard]] static constexpr EmuDuration zero()
	{
		return EmuDuration(uint64_t(0));
	}

	[[nodiscard]] static constexpr EmuDuration infinity()
	{
		return EmuDuration(std::numeric_limits<uint64_t>::max());
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

private:
	uint64_t time = 0;
};

template<> struct SerializeAsMemcpy<EmuDuration> : std::true_type {};


template<std::unsigned_integral T> class EmuDurationCompactStorage
{
public:
	explicit constexpr EmuDurationCompactStorage(EmuDuration e)
		: time(T(e.toUint64()))
	{
		assert(e.toUint64() <= std::numeric_limits<T>::max());
	}

	[[nodiscard]] explicit constexpr operator EmuDuration() const
	{
		return EmuDuration(uint64_t(time));
	}
private:
	T time;
};

using EmuDuration32 = EmuDurationCompactStorage<uint32_t>;
using EmuDuration16 = EmuDurationCompactStorage<uint16_t>;
using EmuDuration8  = EmuDurationCompactStorage<uint8_t>;

template<uint64_t MAX>
using EmuDurationStorageFor = std::conditional_t<(MAX > std::numeric_limits<uint32_t>::max()), EmuDuration,
                              std::conditional_t<(MAX > std::numeric_limits<uint16_t>::max()), EmuDuration32,
                              std::conditional_t<(MAX > std::numeric_limits<uint8_t >::max()), EmuDuration16,
                                                                                               EmuDuration8>>>;
} // namespace openmsx

#endif
