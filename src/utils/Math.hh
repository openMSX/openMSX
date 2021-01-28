#ifndef MATH_HH
#define MATH_HH

#include "likely.hh"
#include <cassert>
#include <cmath>
#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif

// These constants are very common extensions, but not guaranteed to be defined
// by <cmath> when compiling in a strict standards compliant mode. Also e.g.
// visual studio does not provide them.
#ifndef M_E
#define M_E    2.7182818284590452354
#endif
#ifndef M_LN2
#define M_LN2  0.69314718055994530942 // log_e 2
#endif
#ifndef M_LN10
#define M_LN10 2.30258509299404568402 // log_e 10
#endif
#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif

namespace Math {

/** Returns the number of bits needed to store the value 'x', that is:
  *   for x==0 : 0
  *   for x!=0 : 1 + floor(log2(x))
  * This will be part of c++20:
  *   https://en.cppreference.com/w/cpp/numeric/log2p1
  */
template<typename T>
[[nodiscard]] constexpr T log2p1(T x) noexcept
{
	T result = 0;
	while (x) {
		++result;
		x >>= 1;
	}
	return result;
}

/** Is the given number an integral power of two?
  * That is, does it have exactly one 1-bit in binary representation.
  * (So zero is not a power of two).
  *
  * This will be part of c++20:
  *   https://en.cppreference.com/w/cpp/numeric/ispow2
  */
template<typename T>
[[nodiscard]] constexpr bool ispow2(T x) noexcept
{
	return x && ((x & (x - 1)) == 0);
}

/** Returns the smallest number of the form 2^n-1 that is greater or equal
  * to the given number.
  * The resulting number has the same number of leading zeros as the input,
  * but starting from the first 1-bit in the input all bits more to the right
  * are also 1.
  */
template<typename T>
[[nodiscard]] constexpr T floodRight(T x) noexcept
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> ((sizeof(x) >= 2) ?  8 : 0); // Written in a weird way to
	x |= x >> ((sizeof(x) >= 4) ? 16 : 0); // suppress compiler warnings.
	x |= x >> ((sizeof(x) >= 8) ? 32 : 0); // Generates equally efficient
	return x;                              // code.
}

/** Returns the smallest number that is both >=a and a power of two.
  * This will be part of c++20:
  *   https://en.cppreference.com/w/cpp/numeric/ceil2
  */
template<typename T>
[[nodiscard]] constexpr T ceil2(T x) noexcept
{
	// classical implementation:
	//   unsigned res = 1;
	//   while (x > res) res <<= 1;
	//   return res;

	// optimized version
	x += (x == 0); // can be removed if argument is never zero
	return floodRight(x - 1) + 1;
}

/** Clip x to range [-32768,32767]. Special case of the version above.
  * Optimized for the case when no clipping is needed.
  */
[[nodiscard]] inline int16_t clipIntToShort(int x)
{
	static_assert((-1 >> 1) == -1, "right-shift must preserve sign");
	return likely(int16_t(x) == x) ? x : (0x7FFF - (x >> 31));
}

/** Clip x to range [0,255].
  * Optimized for the case when no clipping is needed.
  */
[[nodiscard]] inline uint8_t clipIntToByte(int x)
{
	static_assert((-1 >> 1) == -1, "right-shift must preserve sign");
	return likely(uint8_t(x) == x) ? x : ~(x >> 31);
}

/** Reverse the lower N bits of a given value.
  * The upper 32-N bits from the input are ignored and will be returned as 0.
  * For example reverseNBits('xxxabcde', 5) returns '000edcba' (binary notation).
  */
[[nodiscard]] constexpr unsigned reverseNBits(unsigned x, unsigned bits)
{
	unsigned ret = 0;
	while (bits--) {
		ret = (ret << 1) | (x & 1);
		x >>= 1;
	}
	return ret;

	/* Just for fun I tried the asm version below (the carry-flag trick
	 * cannot be described in plain C). It's correct and generates shorter
	 * code (both less instructions and less bytes). But it doesn't
	 * actually run faster on the machine I tested on, or only a tiny bit
	 * (possibly because of dependency chains and processor stalls???).
	 * However a big disadvantage of this asm version is that when called
	 * with compile-time constant arguments, this version performs exactly
	 * the same, while the version above can be further optimized by the
	 * compiler (constant-propagation, loop unrolling, ...).
	unsigned ret = 0;
	if (bits) {
		asm (
		"1:	shr	%[VAL]\n"
		"	adc	%[RET],%[RET]\n"
		"	dec	%[BITS]\n"
		"	jne	1b\n"
			: [VAL]  "+r" (val)
			, [BITS] "+r" (bits)
			, [RET]  "+r" (ret)
		);
	}
	return ret;
	*/

	/* Maarten suggested the following approach with O(lg(N)) time
	 * complexity (the version above is O(N)).
	 *  - reverse full (32-bit) word: O(lg(N))
	 *  - shift right over 32-N bits: O(1)
	 * Note: In some lower end CPU the shift-over-N-bits instruction itself
	 *       is O(N), in that case this whole algorithm is O(N)
	 * Note2: Instead of '32' it's also possible to use a lower power of 2,
	 *        as long as it's bigger than or equal to N.
	 * This algorithm may or may not be faster than the version above, I
	 * didn't try it yet. Also because this routine is _NOT_ performance
	 * critical _AT_ALL_ currently.
	 */
}

/** Reverse the bits in a byte.
  * This is equivalent to (but faster than) reverseNBits(x, 8);
  */
[[nodiscard]] constexpr uint8_t reverseByte(uint8_t a)
{
	// Classical implementation (can be extended to 16 and 32 bits)
	//   a = ((a & 0xF0) >> 4) | ((a & 0x0F) << 4);
	//   a = ((a & 0xCC) >> 2) | ((a & 0x33) << 2);
	//   a = ((a & 0xAA) >> 1) | ((a & 0x55) << 1);
	//   return a;

	// The versions below are specific to reverse a single byte (can't
	// easily be extended to wider types). Found these tricks on:
	//    http://graphics.stanford.edu/~seander/bithacks.html
#ifdef __x86_64
	// on 64-bit systems this is slightly faster
	return (((a * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL) >> 32;
#else
	// on 32-bit systems this is faster
	return (((a * 0x0802 & 0x22110) | (a * 0x8020 & 0x88440)) * 0x10101) >> 16;
#endif
}

/** Count the number of leading zero-bits in the given word.
  * The result is undefined when the input is zero (all bits are zero).
  */
[[nodiscard]] constexpr unsigned countLeadingZeros(unsigned x)
{
#ifdef __GNUC__
	// actually this only exists starting from gcc-3.4.x
	return __builtin_clz(x); // undefined when x==0
#else
	// gives incorrect result for x==0, but that doesn't matter here
	unsigned lz = 0;
	if (x <= 0x0000ffff) { lz += 16; x <<= 16; }
	if (x <= 0x00ffffff) { lz +=  8; x <<=  8; }
	if (x <= 0x0fffffff) { lz +=  4; x <<=  4; }
	lz += (0x55ac >> ((x >> 27) & 0x1e)) & 0x3;
	return lz;
#endif
}

/** Find the least significant bit that is set.
  * @return 0 if the input is zero (no bits are set),
  *   otherwise the index of the first set bit + 1.
  */
[[nodiscard]] inline /*constexpr*/ unsigned findFirstSet(unsigned x)
{
#if defined(__GNUC__)
	return __builtin_ffs(x);
#elif defined(_MSC_VER)
	unsigned long index;
	return _BitScanForward(&index, x) ? index + 1 : 0;
#else
	if (x == 0) return 0;
	int pos = 0;
	if ((x & 0xffff) == 0) { pos += 16; x >>= 16; }
	if ((x & 0x00ff) == 0) { pos +=  8; x >>=  8; }
	if ((x & 0x000f) == 0) { pos +=  4; x >>=  4; }
	if ((x & 0x0003) == 0) { pos +=  2; x >>=  2; }
	if ((x & 0x0001) == 0) { pos +=  1; }
	return pos + 1;
#endif
}

// Cubic Hermite Interpolation:
//   Given 4 points: (-1, y[-1]), (0, y[0]), (1, y[1]), (2, y[2])
//   Fit a polynomial:  f(x) = a*x^3 + b*x^2 + c*x + d
//     which passes through the given points at x=0 and x=1
//       f(0) = y[0]
//       f(1) = y[1]
//     and which has specific derivatives at x=0 and x=1
//       f'(0) = (y[1] - y[-1]) / 2
//       f'(1) = (y[2] - y[ 0]) / 2
//   Then evaluate this polynomial at the given x-position (x in [0, 1]).
// For more details see:
//   https://en.wikipedia.org/wiki/Cubic_Hermite_spline
//   https://www.paulinternet.nl/?page=bicubic
[[nodiscard]] constexpr float cubicHermite(const float* y, float x)
{
	assert(0.0f <= x); assert(x <= 1.0f);
	float a = -0.5f*y[-1] + 1.5f*y[0] - 1.5f*y[1] + 0.5f*y[2];
	float b =       y[-1] - 2.5f*y[0] + 2.0f*y[1] - 0.5f*y[2];
	float c = -0.5f*y[-1]             + 0.5f*y[1];
	float d =                    y[0];
	float x2 = x * x;
	float x3 = x * x2;
	return a*x3 + b*x2 + c*x + d;
}

} // namespace Math

#endif // MATH_HH
