#ifndef DIVMODBYCONST_HH
#define DIVMODBYCONST_HH

#include "uint128.hh"
#include <bit>
#include <cstdint>
#include <type_traits>

/** Utility class to optimize 64-bit divide/module by a 32-bit constant.
 * For 32-bit by 32-bit gcc already does this optimization (on 64-bit
 * CPUs gcc also does it for 64-bit operands). This optimization especially
 * helps on CPU without a HW division instruction (like ARM).
 *
 * Usage:
 *   DivModByConst<123> dm;
 *   uint32_t d = dm.div(x);  // equivalent to  d = x / 123;
 *   uint32_t m = dm.mod(x);  // equivalent to  d = x % 123;
 */

namespace DivModByConstPrivate {

struct Reduce0Result {
	uint32_t divisor, shift;
};
[[nodiscard]] constexpr Reduce0Result reduce0(uint32_t divisor)
{
	uint32_t shift = 0;
	while ((divisor & 1) == 0) {
		divisor >>= 1;
		++shift;
	}
	return {divisor, shift};
}

struct Reduce1Result {
	uint64_t m;
	uint32_t s;
};
[[nodiscard]] constexpr Reduce1Result reduce1(uint64_t m, uint32_t s)
{
	while (!(m & 1)) {
		m >>= 1;
		--s;
	}
	return {m, s};
}

struct Reduce2Result {
	uint128 m_low, m_high;
	uint32_t l;
};
[[nodiscard]] constexpr Reduce2Result reduce2(uint128 m_low, uint128 m_high, uint32_t l)
{
	while (((m_low >> 1) < (m_high >> 1)) && (l > 0)) {
		m_low  >>= 1;
		m_high >>= 1;
		--l;
	}
	return {m_low, m_high, l};
}


[[nodiscard]] static constexpr uint64_t mla64(uint64_t a, uint64_t b, uint64_t c)
{
	// equivalent to this:
	//    return (__uint128_t(a) * b + c) >> 64;
	uint64_t t1 = uint64_t(uint32_t(a)) * uint32_t(b);
	uint64_t t2 = (a >> 32) * uint32_t(b);
	uint64_t t3 = uint32_t(a) * (b >> 32);
	uint64_t t4 = (a >> 32) * (b >> 32);

	uint64_t s1 = uint64_t(uint32_t(c)) + uint32_t(t1);
	uint64_t s2 = (s1 >> 32) + (c >> 32) + (t1 >> 32) + t2;
	uint64_t s3 = uint64_t(uint32_t(s2)) + uint32_t(t3);
	uint64_t s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;
	return s4;
}

template<uint32_t DIVISOR>
[[nodiscard]] constexpr auto getAlgorithm()
{
	// reduce to odd divisor
	constexpr auto r0 = reduce0(DIVISOR);
	if constexpr (r0.divisor == 1) {
		// algorithm 1: division possible by only shifting
		constexpr uint32_t shift = r0.shift;
		return [=](uint64_t dividend) {
			return dividend >> shift;
		};
	} else {
		constexpr uint32_t L = std::bit_width(r0.divisor);
		constexpr uint64_t J = 0xffffffffffffffffULL % r0.divisor;
		constexpr uint128 L64 = uint128(1) << (L + 64);
		constexpr uint128 k = L64 / (0xffffffffffffffffULL - J);
		constexpr uint128 m_low = L64 / r0.divisor;
		constexpr uint128 m_high = (L64 + k) / r0.divisor;
		constexpr auto r2 = reduce2(m_low, m_high, L);

		if constexpr (high64(r2.m_high) == 0) {
			// algorithm 2: division possible by multiplication and shift
			constexpr auto r1 = reduce1(low64(r2.m_high), r0.shift + r2.l);

			constexpr uint64_t mul = r1.m;
			constexpr uint32_t shift = r1.s;
			return [=](uint64_t dividend) {
				return mla64(dividend, mul, 0) >> shift;
			};
		} else {
			// algorithm 3: division possible by multiplication, addition and shift
			constexpr uint32_t S = std::bit_width(r0.divisor) - 1;
			constexpr uint128 S64 = uint128(1) << (S + 64);
			constexpr uint128 dq = S64 / r0.divisor;
			constexpr uint128 dr = S64 % r0.divisor;
			constexpr uint64_t M = low64(dq) + (low64(dr) > (r0.divisor / 2));
			constexpr auto r1 = reduce1(M, S + r0.shift);

			constexpr uint64_t mul = r1.m;
			constexpr uint32_t shift = r1.s;
			return [=](uint64_t dividend) {
				return mla64(dividend, mul, mul) >> shift;
			};
		}
	}
}

} // namespace DivModByConstPrivate


template<uint32_t DIVISOR> struct DivModByConst
{
	[[nodiscard]] constexpr uint32_t div(uint64_t dividend) const
	{
	#ifdef __x86_64
		// on 64-bit CPUs gcc already does this
		// optimization (and better)
		uint64_t result = dividend / DIVISOR;
	#else
		auto algorithm = DivModByConstPrivate::getAlgorithm<DIVISOR>();
		uint64_t result = algorithm(dividend);
	#endif
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	}

	[[nodiscard]] constexpr uint32_t mod(uint64_t dividend) const
	{
	#ifdef __x86_64
		uint64_t result = dividend % DIVISOR;
	#else
		uint64_t result = dividend - DIVISOR * div(dividend);
	#endif
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	}
};

#endif // DIVMODBYCONST_HH
