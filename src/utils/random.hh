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

#endif
