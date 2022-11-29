#ifndef XRANGE_HH
#define XRANGE_HH

// Utility to iterate over a range of numbers,
// modeled after python's xrange() function.
//
// Typically used as a more compact notation for some for-loop patterns:
//
// a) loop over [0, N)
//
//   auto num = expensiveFunctionCall();
//   for (decltype(num) i = 0; i < num; ++i) { ... }
//
//   for (auto i : xrange(expensiveFunctionCall()) { ... }
//
// b) loop over [B, E)
//
//   auto i   = func1();
//   auto end = func2();
//   for (/**/; i < end; ++i) { ... }
//
//   for (auto i : xrange(func1(), func2()) { ... }
//
// Note that when using the xrange utility you also don't need to worry about
// the correct type of the induction variable.
//
// Gcc is able to optimize the xrange() based loops to the same code as the
// equivalent 'old-style' for loop.
//
// In an earlier version of this utility I also implemented the possibility to
// specify a step size (currently the step size is always +1). Although I
// believe that code was correct I still removed it because it was quite tricky
// (getting the stop condition correct is not trivial) and we don't need it
// currently.

#include "narrow.hh"
#include <cstddef>
#include <iterator>
#include <type_traits>

template<typename T> struct XRange
{
	struct Iter
	{
		using difference_type = ptrdiff_t;
		using value_type = T;
		using pointer    = T*;
		using reference  = T&;
		using iterator_category = std::random_access_iterator_tag;

		// ForwardIterator
		[[nodiscard]] constexpr T operator*() const
		{
			return x;
		}

		constexpr Iter& operator++()
		{
			++x;
			return *this;
		}
		constexpr Iter operator++(int)
		{
			auto copy = *this;
			++x;
			return copy;
		}

		[[nodiscard]] /*constexpr*/ bool operator==(const Iter&) const = default;

		// BidirectionalIterator
		constexpr Iter& operator--()
		{
			--x;
			return *this;
		}
		constexpr Iter operator--(int)
		{
			auto copy = *this;
			--x;
			return copy;
		}

		// RandomAccessIterator
		constexpr Iter& operator+=(difference_type n)
		{
			x += narrow<T>(n);
			return *this;
		}
		constexpr Iter& operator-=(difference_type n)
		{
			x -= narrow<T>(n);
			return *this;
		}

		[[nodiscard]] constexpr friend Iter operator+(Iter i, difference_type n)
		{
			i += n;
			return i;
		}
		[[nodiscard]] constexpr friend Iter operator+(difference_type n, Iter i)
		{
			i += n;
			return i;
		}
		[[nodiscard]] constexpr friend Iter operator-(Iter i, difference_type n)
		{
			i -= n;
			return i;
		}

		[[nodiscard]] constexpr friend difference_type operator-(const Iter& i, const Iter& j)
		{
			return i.x - j.x;
		}

		[[nodiscard]] constexpr T operator[](difference_type n)
		{
			return *(*this + n);
		}

		[[nodiscard]] /*constexpr*/ auto operator<=>(const Iter&) const = default;

		T x;
	};

	[[nodiscard]] constexpr auto begin() const { return Iter{b}; }
	[[nodiscard]] constexpr auto end()   const { return Iter{e}; }

	/*const*/ T b; // non-const to workaround msvc bug(?)
	/*const*/ T e;
};

template<typename T> [[nodiscard]] constexpr auto xrange(T e)
{
	return XRange<T>{T(0), e < T(0) ? T(0) : e};
}
template<typename T1, typename T2> [[nodiscard]] constexpr auto xrange(T1 b, T2 e)
{
	static_assert(std::is_signed_v<T1> == std::is_signed_v<T2>);
	using T = std::common_type_t<T1, T2>;
	return XRange<T>{b, e < b ? b : e};
}


/** Repeat the given operation 'op' 'n' times.
 */
template<typename T, typename Op>
constexpr void repeat(T n, Op op)
{
    for (auto i : xrange(n)) {
        (void)i;
        op();
    }
}

#endif
