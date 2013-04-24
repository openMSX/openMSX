#if defined __x86_64 && !defined _MSC_VER

// nothing

#else // __x86_64 && !_MSC_VER

#include "uint128.hh"

uint128& uint128::operator*=(const uint128& b)
{
	uint128 a = *this;
	uint128 t = b;

	lo = 0;
	hi = 0;
	while (t != 0) {
		if (t.lo & 1) {
			*this += a;
		}
		a <<= 1;
		t >>= 1;
	}
	return *this;
}

uint128 uint128::div(const uint128& ds, uint128& remainder) const
{
	uint128 dd = *this;

	uint128 r = 0;
	uint128 q = 0;

	unsigned b = 127;
	while (r < ds) {
		r <<= 1;
		if (dd.bit(b--)) {
			r.lo |= 1;
		}
	}
	++b;

	while (true) {
		if (r < ds) {
			if (!(b--)) break;
			r <<= 1;
			if (dd.bit(b)) {
				r.lo |= 1;
			}
		} else {
			r -= ds;
			q.setBit(b);
		}
	}
	remainder = r;
	return q;
}

bool uint128::bit(unsigned n) const
{
	if (n < 64) {
		return (lo & (1ull << n)) != 0;
	} else {
		return (hi & (1ull << (n - 64))) != 0;
	}
}

void uint128::setBit(unsigned n)
{
	if (n < 64) {
		lo |= (1ull << n);
	} else {
		hi |= (1ull << (n - 64));
	}
}

#endif // __x86_64 && !_MSC_VER
