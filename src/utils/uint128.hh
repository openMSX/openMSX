#ifndef UINT128_HH
#define UINT128_HH

#include <cstdint>

#if defined __x86_64 && !defined _MSC_VER
// On 64-bit CPUs gcc already provides a 128-bit type,
// use that type because it's most likely much more efficient.
// VC++ 2008 does not provide a 128-bit integer type
using uint128 = __uint128_t;
inline uint64_t toUint64(uint128 a) { return a; }

#else // __x86_64 && !_MSC_VER

/** Unsigned 128-bit integer type.
 * Very simple implementation, not optimized for speed.
 *
 * Loosely based on the 128-bit utility classes written by
 *  Jan Ringos, http://Tringi.Mx-3.cz
 */
class uint128
{
public:
	uint128(const uint128& a) : lo(a.lo), hi(a.hi) {}
	uint128(uint64_t a)       : lo(a), hi(0) {}

	bool operator!() const
	{
		return !(hi || lo);
	}

	uint128 operator~() const
	{
		return uint128(~lo, ~hi);
	}
	uint128 operator-() const
	{
		uint128 result = ~*this;
		++result;
		return result;
	}

	uint128& operator++()
	{
		++lo;
		if (lo == 0) ++hi;
		return *this;
	}
	uint128& operator--()
	{
		if (lo == 0) --hi;
		--lo;
		return *this;
	}
	uint128 operator++(int)
	{
		uint128 b = *this;
		++*this;
		return b;
	}
	uint128 operator--(int)
	{
		uint128 b = *this;
		--*this;
		return b;
	}

	uint128& operator+=(const uint128& b)
	{
		uint64_t old_lo = lo;
		lo += b.lo;
		hi += b.hi + (lo < old_lo);
		return *this;
	}
	uint128& operator-=(const uint128& b)
	{
		return *this += (-b);
	}

	uint128& operator>>=(unsigned n)
	{
		n &= 0x7F;
		if (n < 64) {
			lo = (hi << (64 - n)) | (lo >> n);
			hi >>= n;
		} else {
			lo = hi >> (n - 64);
			hi = 0;
		}
		return *this;
	}
	uint128& operator<<=(unsigned n)
	{
		n &= 0x7F;
		if (n < 64) {
			hi = (hi << n) | (lo >> (64 - n));
			lo <<= n;
		} else {
			hi = lo << (n - 64);
			lo = 0;
		}
		return *this;
	}

	uint128& operator|=(const uint128& b)
	{
		hi |= b.hi;
		lo |= b.lo;
		return *this;
	}
	uint128& operator&=(const uint128& b)
	{
		hi &= b.hi;
		lo &= b.lo;
		return *this;
	}
	uint128& operator^=(const uint128& b)
	{
		hi ^= b.hi;
		lo ^= b.lo;
		return *this;
	}

	uint128& operator/=(const uint128& b)
	{
		uint128 dummy;
		*this = div(b, dummy);
		return *this;
	}
	uint128& operator%=(const uint128& b)
	{
		div(b, *this);
		return *this;
	}

	uint128& operator*=(const uint128& b);

private:
	uint128() {}
	uint128(uint64_t a, uint64_t b) : lo(a), hi(b) {}
	uint128 div(const uint128& ds, uint128& remainder) const;
	bool bit(unsigned n) const;
	void setBit(unsigned n);

	uint64_t lo;
	uint64_t hi;

	friend bool operator< (const uint128&, const uint128&);
	friend bool operator==(const uint128&, const uint128&);
	friend bool operator||(const uint128&, const uint128&);
	friend bool operator&&(const uint128&, const uint128&);
	friend uint64_t toUint64(const uint128&);
};


inline uint128 operator+(const uint128& a, const uint128& b)
{
	return uint128(a) += b;
}
inline uint128 operator-(const uint128& a, const uint128& b)
{
	return uint128(a) -= b;
}
inline uint128 operator*(const uint128& a, const uint128& b)
{
	return uint128(a) *= b;
}
inline uint128 operator/(const uint128& a, const uint128& b)
{
	return uint128(a) /= b;
}
inline uint128 operator%(const uint128& a, const uint128& b)
{
	return uint128(a) %= b;
}

inline uint128 operator>>(const uint128& a, unsigned n)
{
	return uint128(a) >>= n;
}
inline uint128 operator<<(const uint128 & a, unsigned n)
{
	return uint128(a) <<= n;
}

inline uint128 operator&(const uint128& a, const uint128& b)
{
	return uint128(a) &= b;
}
inline uint128 operator|(const uint128& a, const uint128& b)
{
	return uint128(a) |= b;
}
inline uint128 operator^(const uint128& a, const uint128& b)
{
	return uint128(a) ^= b;
}

inline bool operator<(const uint128& a, const uint128& b)
{
	return (a.hi == b.hi) ? (a.lo < b.lo) : (a.hi < b.hi);
}
inline bool operator>(const uint128& a, const uint128& b)
{
	return b < a;
}
inline bool operator<=(const uint128& a, const uint128& b)
{
	return !(b < a);
}
inline bool operator>=(const uint128& a, const uint128& b)
{
	return !(a < b);
}
inline bool operator==(const uint128& a, const uint128& b)
{
	return (a.hi == b.hi) && (a.lo == b.lo);
}
inline bool operator!=(const uint128& a, const uint128& b)
{
	return !(a == b);
}

inline bool operator&&(const uint128& a, const uint128& b)
{
	return (a.hi || a.lo) && (b.hi || b.lo);
}
inline bool operator||(const uint128& a, const uint128& b)
{
	return a.hi || a.lo || b.hi || b.lo;
}

inline uint64_t toUint64(const uint128& a)
{
	return a.lo;
}

#endif // __x86_64 && !_MSC_VER

#endif // UINT128_HH
