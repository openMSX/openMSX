#ifndef RANDOM_HH
#define RANDOM_HH

#include <random>

/** Return reference to a (shared) global random number generator.
  */
[[nodiscard]] inline auto& global_urng()
{
  static std::minstd_rand0 u;
  return u;
}

/** Seed the (shared) random number generator.
  */
inline void randomize()
{
  static std::random_device rd;
  global_urng().seed(rd());
}

/** Return a random boolean value.
  */
[[nodiscard]] inline bool random_bool()
{
  // Note: this is only 100% uniform if 'generator.max() -
  // generator().min() + 1' is even. This is the case for
  // std::minstd_rand0.
  auto& generator = global_urng();
  return generator() & 1;
}

/** Return a random integer in the range [from, thru] (note: closed interval).
  * This function is convenient if you only need a few random values. If you
  * need a large amount it's a bit faster to create a local distribution
  * object and reuse that for all your values.
  */
[[nodiscard]] inline int random_int(int from, int thru)
{
  static std::uniform_int_distribution<int> d;
  using parm_t = decltype(d)::param_type;
  return d(global_urng(), parm_t{from, thru});
}

/** Return a random float in the range [from, upto) (note: half-open interval).
  * This function is convenient if you only need a few random values. If you
  * need a large amount it's a bit faster to create a local distribution
  * object and reuse that for all your values.
  */
[[nodiscard]] inline float random_float(float from, float upto)
{
  static std::uniform_real_distribution<float> d;
  using parm_t = decltype(d)::param_type;
  return d(global_urng(), parm_t{from, upto});
}

/** Return a random 32-bit value.
  * This function should rarely be used. It should NOT be used if you actually
  * need random values in a smaller range than [0 .. 0xffffffff]. For example:
  * 'random_32bit % N' with
  *  - N not a power-of-2: is NOT a uniform distribution anymore (higher values
  *     have a slightly lower probability than the lower values)
  *  - N a power-of-2: does more work than needed (typically has to call the
  *     underlying generator more than once to make sure all 32 bits are
  *     random, but then those upper bits are discarded).
  */
[[nodiscard]] inline uint32_t random_32bit()
{
  static std::uniform_int_distribution<uint32_t> d;
  using parm_t = decltype(d)::param_type;
  return d(global_urng(), parm_t{0, 0xffffffff});
}


// Compile-time random numbers

// Constexpr "Permuted Congruential Generator" (PCG)
//   generated random numbers at compile-time
//   (or it shifts the problem to obtaining a random seed at compile time)
// Based upon:
//   C++ Weekly - Ep 44 - constexpr Compile Time Random
//   https://www.youtube.com/watch?v=rpn_5Mrrxf8
template<uint64_t SEED>
struct PCG
{
	constexpr uint32_t operator()()
	{
		// Advance internal state
		auto oldState = state;
		state = oldState * 6364136223846793005ULL + (SEED | 1);

		// Calculate output function (XSH RR), uses old state for max
		// ILP
		uint32_t xorShifted = ((oldState >> 18) ^ oldState) >> 27;
		uint32_t rot = oldState >> 59;
		return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
	}

private:
	uint64_t state = SEED;
};

// Turns a random 32-bit integer into a random float in the range [0, 1).
[[nodiscard]] constexpr float getCanonicalFloat(uint32_t u)
{
	uint32_t b = 1 << 23;
	uint32_t m = b - 1;
	return float(u & m) / float(b);
}

#endif
