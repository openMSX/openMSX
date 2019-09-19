#ifndef EMUDURATION_HH
#define EMUDURATION_HH

#include "serialize.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

// constants
static const uint64_t MAIN_FREQ = 3579545ULL * 960;
static const unsigned MAIN_FREQ32 = MAIN_FREQ;
static_assert(MAIN_FREQ < (1ull << 32), "must fit in 32 bit");


class EmuDuration
{
public:
	// This is only a very small class (one 64-bit member). On 64-bit CPUs
	// it's cheaper to pass this by value. On 32-bit CPUs pass-by-reference
	// is cheaper.
#ifdef __x86_64
	using param = const EmuDuration;
#else
	using param = const EmuDuration&;
#endif

	// friends
	friend class EmuTime;

	// constructors
	EmuDuration() = default;
	explicit EmuDuration(uint64_t n) : time(n) {}
	explicit EmuDuration(double duration)
		: time(uint64_t(duration * MAIN_FREQ)) {}

	static EmuDuration sec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ); }
	static EmuDuration msec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ / 1000); }
	static EmuDuration usec(unsigned x)
		{ return EmuDuration(x * MAIN_FREQ / 1000000); }
	static EmuDuration hz(unsigned x)
		{ return EmuDuration(MAIN_FREQ / x); }

	// conversions
	double toDouble() const { return double(time) / MAIN_FREQ32; }
	uint64_t length() const { return time; }

	// comparison operators
	bool operator==(EmuDuration::param d) const
		{ return time == d.time; }
	bool operator!=(EmuDuration::param d) const
		{ return time != d.time; }
	bool operator< (EmuDuration::param d) const
		{ return time <  d.time; }
	bool operator<=(EmuDuration::param d) const
		{ return time <= d.time; }
	bool operator> (EmuDuration::param d) const
		{ return time >  d.time; }
	bool operator>=(EmuDuration::param d) const
		{ return time >= d.time; }

	// arithmetic operators
	EmuDuration operator%(EmuDuration::param d) const
		{ return EmuDuration(time % d.time); }
	EmuDuration operator+(EmuDuration::param d) const
		{ return EmuDuration(time + d.time); }
	EmuDuration operator*(unsigned fact) const
		{ return EmuDuration(time * fact); }
	EmuDuration operator/(unsigned fact) const
		{ return EmuDuration(time / fact); }
	EmuDuration divRoundUp(unsigned fact) const
		{ return EmuDuration((time + fact - 1) / fact); }
	unsigned operator/(EmuDuration::param d) const
	{
		uint64_t result = time / d.time;
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	unsigned divUp(EmuDuration::param d) const {
		uint64_t result = (time + d.time - 1) / d.time;
#ifdef DEBUG
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	double div(EmuDuration::param d) const
		{ return double(time) / d.time; }

	EmuDuration& operator*=(unsigned fact)
		{ time *= fact; return *this; }
	EmuDuration& operator*=(double fact)
		{ time = uint64_t(time * fact); return *this; }
	EmuDuration& operator/=(double fact)
		{ time = uint64_t(time / fact); return *this; }

	// ticks
	// TODO: Used in WavAudioInput. Keep or use DynamicClock instead?
	unsigned getTicksAt(unsigned freq) const
	{
		uint64_t result = time / (MAIN_FREQ32 / freq);
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	static const EmuDuration zero;
	static const EmuDuration infinity;

private:
	uint64_t time = 0;
};

template<> struct SerializeAsMemcpy<EmuDuration> : std::true_type {};

} // namespace openmsx

#endif
