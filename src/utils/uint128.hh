#ifndef UINT128_HH
#define UINT128_HH

#include "narrow.hh"
#include <cstdint>
#include <tuple>
#include <utility>

#if defined __x86_64 && !defined _MSC_VER
// On 64-bit CPUs gcc already provides a 128-bit type,
// use that type because it's most likely much more efficient.
// VC++ 2008 does not provide a 128-bit integer type
using uint128 = __uint128_t;
[[nodiscard]] constexpr uint64_t low64 (uint128 a) { return narrow_cast<uint64_t>(a >>  0); }
[[nodiscard]] constexpr uint64_t high64(uint128 a) { return narrow_cast<uint64_t>(a >> 64); }

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
	constexpr uint128(uint64_t a) : lo(a) {}

	[[nodiscard]] constexpr bool operator==(const uint128&) const = default;

	[[nodiscard]] constexpr bool operator!() const
	{
		return !(hi || lo);
	}

	[[nodiscard]] constexpr uint128 operator~() const
	{
		return {~lo, ~hi};
	}
	[[nodiscard]] constexpr uint128 operator-() const
	{
		uint128 result = ~*this;
		++result;
		return result;
	}

	constexpr uint128& operator++()
	{
		++lo;
		if (lo == 0) ++hi;
		return *this;
	}
	constexpr uint128& operator--()
	{
		if (lo == 0) --hi;
		--lo;
		return *this;
	}
	constexpr uint128 operator++(int)
	{
		uint128 b = *this;
		++*this;
		return b;
	}
	constexpr uint128 operator--(int)
	{
		uint128 b = *this;
		--*this;
		return b;
	}

	constexpr uint128& operator+=(const uint128& b)
	{
		uint64_t old_lo = lo;
		lo += b.lo;
		hi += b.hi + (lo < old_lo);
		return *this;
	}
	constexpr uint128& operator-=(const uint128& b)
	{
		return *this += (-b);
	}

	constexpr uint128& operator>>=(unsigned n)
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
	constexpr uint128& operator<<=(unsigned n)
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

	constexpr uint128& operator|=(const uint128& b)
	{
		hi |= b.hi;
		lo |= b.lo;
		return *this;
	}
	constexpr uint128& operator&=(const uint128& b)
	{
		hi &= b.hi;
		lo &= b.lo;
		return *this;
	}
	constexpr uint128& operator^=(const uint128& b)
	{
		hi ^= b.hi;
		lo ^= b.lo;
		return *this;
	}

	constexpr uint128& operator/=(const uint128& b)
	{
		auto [q, r] = div(b);
		*this = q;
		return *this;
	}
	constexpr uint128& operator%=(const uint128& b)
	{
		auto [q, r] = div(b);
		*this = r;
		return *this;
	}

	constexpr uint128& operator*=(const uint128& b);

private:
	constexpr uint128() = default;
	constexpr uint128(uint64_t low, uint64_t high) : lo(low), hi(high) {}

	[[nodiscard]] constexpr std::pair<uint128, uint128> div(const uint128& ds) const;

	[[nodiscard]] constexpr bool bit(unsigned n) const
	{
		if (n < 64) {
			return (lo & (1ULL << n)) != 0;
		} else {
			return (hi & (1ULL << (n - 64))) != 0;
		}
	}

	constexpr void setBit(unsigned n)
	{
		if (n < 64) {
			lo |= (1ULL << n);
		} else {
			hi |= (1ULL << (n - 64));
		}
	}

	[[nodiscard]] constexpr friend uint128 operator+(const uint128& a, const uint128& b)
	{
		return uint128(a) += b;
	}
	[[nodiscard]] constexpr friend uint128 operator-(const uint128& a, const uint128& b)
	{
		return uint128(a) -= b;
	}
	[[nodiscard]] constexpr friend uint128 operator*(const uint128& a, const uint128& b)
	{
		return uint128(a) *= b;
	}
	[[nodiscard]] constexpr friend uint128 operator/(const uint128& a, const uint128& b)
	{
		return uint128(a) /= b;
	}
	[[nodiscard]] constexpr friend uint128 operator%(const uint128& a, const uint128& b)
	{
		return uint128(a) %= b;
	}

	[[nodiscard]] constexpr friend uint128 operator>>(const uint128& a, unsigned n)
	{
		return uint128(a) >>= n;
	}
	[[nodiscard]] constexpr friend uint128 operator<<(const uint128 & a, unsigned n)
	{
		return uint128(a) <<= n;
	}

	[[nodiscard]] constexpr friend uint128 operator&(const uint128& a, const uint128& b)
	{
		return uint128(a) &= b;
	}
	[[nodiscard]] constexpr friend uint128 operator|(const uint128& a, const uint128& b)
	{
		return uint128(a) |= b;
	}
	[[nodiscard]] constexpr friend uint128 operator^(const uint128& a, const uint128& b)
	{
		return uint128(a) ^= b;
	}

	[[nodiscard]] constexpr friend auto operator<=>(const uint128& a, const uint128& b)
	{
		// Doesn't work yet with clang-13 libc++
		//return std::tuple(high64(a), low64(a)) <=>
		//       std::tuple(high64(b), low64(b));
		if (auto cmp = high64(a) <=> high64(b); cmp != 0) return cmp;
		return low64(a) <=> low64(b);
	}

	[[nodiscard]] friend constexpr uint64_t low64(const uint128& a)
	{
		return a.lo;
	}

	[[nodiscard]] friend constexpr uint64_t high64(const uint128& a)
	{
		return a.hi;
	}

private:
	uint64_t lo = 0;
	uint64_t hi = 0;
};

[[nodiscard]] constexpr bool operator&&(const uint128& a, const uint128& b)
{
	return !!a && !!b;
}
[[nodiscard]] constexpr bool operator||(const uint128& a, const uint128& b)
{
	return !!a || !!b;
}

constexpr uint128& uint128::operator*=(const uint128& b)
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

constexpr std::pair<uint128, uint128> uint128::div(const uint128& ds) const
{
	assert(ds != uint128(0));

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
	return {q, r};
}

#endif // __x86_64 && !_MSC_VER

#endif // UINT128_HH
