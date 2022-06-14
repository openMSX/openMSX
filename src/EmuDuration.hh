#ifndef EMUDURATION_HH
#define EMUDURATION_HH

#include "serialize.hh"
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace openmsx {

// constants
constexpr uint64_t MAIN_FREQ = 3579545ULL * 960;
constexpr unsigned MAIN_FREQ32 = MAIN_FREQ;
static_assert(MAIN_FREQ < (1ull << 32), "must fit in 32 bit");


class EmuDuration
{
public:
	// This is only a very small class (one 64-bit member). On 64-bit CPUs
	// it's cheaper to pass this by value. On 32-bit CPUs pass-by-reference
	// is cheaper.
#ifdef __x86_64
	using param = EmuDuration;
#else
	using param = const EmuDuration&;
#endif

	// friends
	friend class EmuTime;

	// constructors
	constexpr EmuDuration() = default;
	constexpr explicit EmuDuration(uint64_t n) : time(n) {}
	constexpr explicit EmuDuration(double duration)
		: time(uint64_t(duration * MAIN_FREQ + 0.5)) {}

	static constexpr EmuDuration sec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ); }
	static constexpr EmuDuration msec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ / 1000); }
	static constexpr EmuDuration usec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ / 1000000); }
	static constexpr EmuDuration hz(unsigned x)
		{ return EmuDuration(MAIN_FREQ / x); }

	// conversions
	[[nodiscard]] constexpr double toDouble() const { return double(time) / MAIN_FREQ32; }
	[[nodiscard]] constexpr uint64_t length() const { return time; }

	// comparison operators
	[[nodiscard]] constexpr bool operator==(EmuDuration::param d) const
		{ return time == d.time; }
	[[nodiscard]] constexpr bool operator< (EmuDuration::param d) const
		{ return time <  d.time; }
	[[nodiscard]] constexpr bool operator<=(EmuDuration::param d) const
		{ return time <= d.time; }
	[[nodiscard]] constexpr bool operator> (EmuDuration::param d) const
		{ return time >  d.time; }
	[[nodiscard]] constexpr bool operator>=(EmuDuration::param d) const
		{ return time >= d.time; }

	// arithmetic operators
	[[nodiscard]] constexpr EmuDuration operator%(EmuDuration::param d) const
		{ return EmuDuration(time % d.time); }
	[[nodiscard]] constexpr EmuDuration operator+(EmuDuration::param d) const
		{ return EmuDuration(time + d.time); }
	[[nodiscard]] constexpr EmuDuration operator*(uint64_t fact) const
		{ return EmuDuration(time * fact); }
	[[nodiscard]] constexpr EmuDuration operator/(unsigned fact) const
		{ return EmuDuration(time / fact); }
	[[nodiscard]] constexpr EmuDuration divRoundUp(unsigned fact) const
		{ return EmuDuration((time + fact - 1) / fact); }
	[[nodiscard]] constexpr unsigned operator/(EmuDuration::param d) const
	{
		uint64_t result = time / d.time;
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	[[nodiscard]] constexpr unsigned divUp(EmuDuration::param d) const {
		uint64_t result = (time + d.time - 1) / d.time;
#ifdef DEBUG
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	[[nodiscard]] constexpr double div(EmuDuration::param d) const
		{ return double(time) / d.time; }

	constexpr EmuDuration& operator*=(unsigned fact)
		{ time *= fact; return *this; }
	constexpr EmuDuration& operator*=(double fact)
		{ time = uint64_t(time * fact); return *this; }
	constexpr EmuDuration& operator/=(double fact)
		{ time = uint64_t(time / fact); return *this; }

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


template<typename T> class EmuDurationCompactStorage
{
public:
	constexpr EmuDurationCompactStorage(EmuDuration e)
		: time(T(e.length()))
	{
		assert(e.length() <= std::numeric_limits<T>::max());
	}

	[[nodiscard]] constexpr operator EmuDuration() const
	{
		return EmuDuration(uint64_t(time));
	}
private:
	T time;
};

using EmuDuration32 = EmuDurationCompactStorage<uint32_t>;
using EmuDuration16 = EmuDurationCompactStorage<uint16_t>;
using EmuDuration8  = EmuDurationCompactStorage<uint8_t>;

namespace detail {
	// via intermediate variable to work around gcc-10 warning
	inline constexpr uint64_t max32 = std::numeric_limits<uint32_t>::max();
	inline constexpr uint64_t max16 = std::numeric_limits<uint16_t>::max();
	inline constexpr uint64_t max8  = std::numeric_limits<uint8_t >::max();
}
template<uint64_t MAX>
using EmuDurationStorageFor = std::conditional_t<(MAX > detail::max32), EmuDuration,
                              std::conditional_t<(MAX > detail::max16), EmuDuration32,
                              std::conditional_t<(MAX > detail::max8 ), EmuDuration16,
                                                                        EmuDuration8>>>;
} // namespace openmsx

#endif
