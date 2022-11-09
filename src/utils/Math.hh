#ifndef MATH_HH
#define MATH_HH

#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <numbers>
#include <span>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
#endif

namespace Math {

inline constexpr double e    = std::numbers::e_v   <double>;
inline constexpr double ln2  = std::numbers::ln2_v <double>;
inline constexpr double ln10 = std::numbers::ln10_v<double>;
inline constexpr double pi   = std::numbers::pi_v  <double>;

/** Returns the smallest number of the form 2^n-1 that is greater or equal
  * to the given number.
  * The resulting number has the same number of leading zeros as the input,
  * but starting from the first 1-bit in the input all bits more to the right
  * are also 1.
  */
[[nodiscard]] constexpr auto floodRight(std::unsigned_integral auto x) noexcept
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> ((sizeof(x) >= 2) ?  8 : 0); // Written in a weird way to
	x |= x >> ((sizeof(x) >= 4) ? 16 : 0); // suppress compiler warnings.
	x |= x >> ((sizeof(x) >= 8) ? 32 : 0); // Generates equally efficient
	return x;                              // code.
}

/** Clip x to range [-32768,32767]. Special case of the version above.
  * Optimized for the case when no clipping is needed.
  */
template<std::signed_integral T>
[[nodiscard]] inline int16_t clipToInt16(T x)
{
	static_assert((-1 >> 1) == -1, "right-shift must preserve sign");
	if (int16_t(x) == x) [[likely]] {
		return x;
	} else {
		return 0x7FFF - (x >> 31);
	}
}

/** Clip x to range [0,255].
  * Optimized for the case when no clipping is needed.
  */
[[nodiscard]] inline uint8_t clipIntToByte(int x)
{
	static_assert((-1 >> 1) == -1, "right-shift must preserve sign");
	if (uint8_t(x) == x) [[likely]] {
		return x;
	} else {
		return ~(x >> 31);
	}
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

/** Find the least significant bit that is set.
  * @return 0 if the input is zero (no bits are set),
  *   otherwise the index of the first set bit + 1.
  */
[[nodiscard]] inline /*constexpr*/ unsigned findFirstSet(uint32_t x)
{
	return x ? std::countr_zero(x) + 1 : 0;
}

// Cubic Hermite Interpolation:
//   Given 4 points: (-1, y[0]), (0, y[1]), (1, y[2]), (2, y[3])
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
[[nodiscard]] constexpr float cubicHermite(std::span<const float, 4> y, float x)
{
	assert(0.0f <= x); assert(x <= 1.0f);
	float a = -0.5f*y[0] + 1.5f*y[1] - 1.5f*y[2] + 0.5f*y[3];
	float b =       y[0] - 2.5f*y[1] + 2.0f*y[2] - 0.5f*y[3];
	float c = -0.5f*y[0]             + 0.5f*y[2];
	float d =                   y[1];
	float x2 = x * x;
	float x3 = x * x2;
	return a*x3 + b*x2 + c*x + d;
}

/** Divide one integer by another, rounding towards minus infinity.
  * The normal C/C++ division rounds towards zero.
  * Based on this article:
  *   http://www.microhowto.info/howto/round_towards_minus_infinity_when_dividing_integers_in_c_or_c++.html
  */
struct QuotientRemainder {
    int quotient;
    int remainder;
};
constexpr QuotientRemainder div_mod_floor(int dividend, int divisor) {
    int q = dividend / divisor;
    int r = dividend % divisor;
    if ((r != 0) && ((r < 0) != (divisor < 0))) {
        --q;
        r += divisor;
    }
    return {q, r};
}
constexpr int div_floor(int dividend, int divisor) {
    return div_mod_floor(dividend, divisor).quotient;
}
constexpr int mod_floor(int dividend, int divisor) {
    return div_mod_floor(dividend, divisor).remainder;
}

} // namespace Math

#endif // MATH_HH
