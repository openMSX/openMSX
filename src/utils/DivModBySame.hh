#ifndef DIVISIONBYCONST_HH
#define DIVISIONBYCONST_HH

#include "build-info.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

/** Helper class to divide multiple times by the same number.
 * Binary division can be performed by:
 *   - multiplication by a magic number
 *   - followed by an addition of a magic number
 *   - follwed by a right shift over some magic number of bits
 * These magic constants only depend on the divisor, but they are quite
 * expensive to calculate.
 * However if you know you will divide many times by the same number this
 * algorithm does make sense and can result in a big speedup, especially on
 * CPUs which lack a hardware division instruction (like ARM).
 *
 * If the divisor is a compile-time constant, it's even faster to use
 * the DivModByConst utility class.
 */
class DivModBySame
{
public:
	void setDivisor(uint32_t divisor);
	inline uint32_t getDivisor() const { return divisor; }

	uint32_t div(uint64_t dividend) const
	{
	#if defined __x86_64 && !defined _MSC_VER
		uint64_t t = (__uint128_t(dividend) * m + a) >> 64;
		return t >> s;
	#else
		return divinC(dividend);
	#endif
	}

	inline uint32_t divinC(uint64_t dividend) const
	{
		uint64_t t1 = uint64_t(uint32_t(dividend)) * uint32_t(m);
		uint64_t t2 = (dividend >> 32) * uint32_t(m);
		uint64_t t3 = uint32_t(dividend) * (m >> 32);
		uint64_t t4 = (dividend >> 32) * (m >> 32);

		uint64_t s1 = uint64_t(uint32_t(a)) + uint32_t(t1);
		uint64_t s2 = (s1 >> 32) + (a >> 32) + (t1 >> 32) + t2;
		uint64_t s3 = uint64_t(uint32_t(s2)) + uint32_t(t3);
		uint64_t s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;

		uint64_t result = s4 >> s;
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	}

	uint32_t mod(uint64_t dividend) const
	{
		assert(uint32_t(divisor) == divisor); // must fit in 32-bit
		uint64_t q = div(dividend);
		assert(uint32_t(q) == q); // must fit in 32 bit
		// result fits in 32-bit, so no 64-bit calculations required
		return uint32_t(dividend) - uint32_t(q) * uint32_t(divisor);
	}

private:
	uint64_t m;
	uint64_t a;
	uint32_t s;
	uint32_t divisor; // only used by mod() and getDivisor()
};

} // namespace openmsx

#endif // DIVISIONBYCONST_HH
