// $Id$

#ifndef EMUDUARTION_HH
#define EMUDUARTION_HH

#include "openmsx.hh"
#include "static_assert.hh"
#include <cassert>

namespace openmsx {

// constants
static const uint64 MAIN_FREQ = 3579545ULL * 960;
static const unsigned MAIN_FREQ32 = MAIN_FREQ;
STATIC_ASSERT(MAIN_FREQ < (1ull << 32));


class EmuDuration
{
public:
	// This is only a very small class (one 64-bit member). On 64-bit CPUs
	// it's cheaper to pass this by value. On 32-bit CPUs pass-by-reference
	// is cheaper.
#ifdef __x86_64
	typedef const EmuDuration param;
#else
	typedef const EmuDuration& param;
#endif

	// friends
	friend class EmuTime;

	// constructors
	EmuDuration()                  : time(0) {}
	explicit EmuDuration(uint64 n) : time(n) {}
	explicit EmuDuration(double duration)
		: time(uint64(duration * MAIN_FREQ)) {}

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
	uint64 length() const { return time; }

	// assignment operator
	EmuDuration& operator=(EmuDuration::param d)
		{ time = d.time; return *this; }

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
	const EmuDuration operator%(EmuDuration::param d) const
		{ return EmuDuration(time % d.time); }
	const EmuDuration operator+(EmuDuration::param d) const
		{ return EmuDuration(time + d.time); }
	const EmuDuration operator*(unsigned fact) const
		{ return EmuDuration(time * fact); }
	const EmuDuration operator/(unsigned fact) const
		{ return EmuDuration(time / fact); }
	const EmuDuration divRoundUp(unsigned fact) const
		{ return EmuDuration((time + fact - 1) / fact); }
	unsigned operator/(EmuDuration::param d) const
	{
		uint64 result = time / d.time;
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	unsigned divUp(EmuDuration::param d) const {
		uint64 result = (time + d.time - 1) / d.time;
#ifdef DEBUG
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	double div(EmuDuration::param d) const
		{ return double(time) / d.time; }

	EmuDuration& operator*=(double fact)
		{ time = uint64(time * fact); return *this; }
	EmuDuration& operator/=(double fact)
		{ time = uint64(time / fact); return *this; }

	// ticks
	// TODO: Used in WavAudioInput. Keep or use DynamicClock instead?
	unsigned getTicksAt(unsigned freq) const
	{
		uint64 result = time / (MAIN_FREQ32 / freq);
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
	uint64 time;
};

} // namespace openmsx

#endif
