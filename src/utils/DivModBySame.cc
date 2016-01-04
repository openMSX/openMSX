#include "DivModBySame.hh"
#include "uint128.hh"

namespace openmsx {

static uint32_t log2(uint64_t i)
{
	uint32_t t = 0;
	i >>= 1;
	while (i) {
		i >>= 1;
		++t;
	}
	return t;
}

void DivModBySame::setDivisor(uint32_t divisor_)
{
	//assert(divisor_ < 0x8000000000000000ull); // when divisor is uint64_t
	divisor = divisor_;

	// reduce divisor until it becomes odd
	uint32_t n = 0;
	uint64_t t = divisor;
	while (!(t & 1)) {
		t >>= 1;
		++n;
	}
	if (t == 1) {
		m = 0xffffffffffffffffull;
		a = m;
		s = 0;
	} else {
		// Generate m, s for algorithm 0. Based on: Granlund, T.; Montgomery,
		// P.L.: "Division by Invariant Integers using Multiplication".
		// SIGPLAN Notices, Vol. 29, June 1994, page 61.
		uint32_t l = log2(t) + 1;
		uint64_t j = 0xffffffffffffffffull % t;
		uint128 k = (uint128(1) << (64 + l)) / (0xffffffffffffffffull - j);
		uint128 m_low  =  (uint128(1) << (64 + l))      / t;
		uint128 m_high = ((uint128(1) << (64 + l)) + k) / t;
		while (((m_low >> 1) < (m_high >> 1)) && (l > 0)) {
			m_low >>= 1;
			m_high >>= 1;
			--l;
		}
		if ((m_high >> 64) == 0) {
			m = toUint64(m_high);
			s = l;
			a = 0;
		} else {
			// Generate m, s for algorithm 1. Based on: Magenheimer, D.J.; et al:
			// "Integer Multiplication and Division on the HP Precision Architecture".
			// IEEE Transactions on Computers, Vol 37, No. 8, August 1988, page 980.
			s = log2(t);
			uint128 m_low2 =      (uint128(1) << (64 + s)) / t;
			uint64_t r = toUint64((uint128(1) << (64 + s)) % t);
			m = toUint64(m_low2 + ((r <= (t >> 1)) ? 0 : 1));
			a = m;
		}
		// reduce multiplier to smallest possible
		while (!(m & 1)) {
			m >>= 1;
			a >>= 1;
			s--;
		}
	}
	// adjust multiplier for reduction of even divisors
	s += n;
}

} // namespace openmsx
