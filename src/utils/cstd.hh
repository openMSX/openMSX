#ifndef CSTD_HH
#define CSTD_HH

#include "string_ref.hh"
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>

namespace cstd {

#if __cplusplus < 201402
// We don't try to provide constexpr algorithms in C++11, only in C++14. It
// should be theoretically possible in C++11, but C++14 lifts some restrictions
// on the use of constexpr that makes this much easier.
#define HAS_CPP14_CONSTEXPR 0
#define CONSTEXPR /**/

#else

#define HAS_CPP14_CONSTEXPR 1
#define CONSTEXPR constexpr

// Inspired by this post:
//    http://tristanbrindle.com/posts/a-more-useful-compile-time-quicksort

// Constexpr reimplementations of standard algorithms or data-structures.
//
// Everything implemented in 'cstd' will very likely become part of standard
// C++ in the future. Or it already is part of C++17. So in the future we can
// remove our (re)implementation and change the callers from 'cstd::xxx()' to
// 'std::xxx()'.

//
// Various constexpr reimplementation of STL algorithms.
//
// Many of these are already constexpr in C++17, or are proposed to be made
// constexpr for the next C++ standard.
//

// begin/end are already constexpr in c++17
template<typename Container>
constexpr auto begin(Container&& c)
{
	return c.begin();
}

template<typename Container>
constexpr auto end(Container&& c)
{
	return c.end();
}


template<typename Iter1, typename Iter2>
constexpr void iter_swap(Iter1 a, Iter2 b)
{
	auto temp = std::move(*a);
	*a = std::move(*b);
	*b = std::move(temp);
}

template<typename InputIt, typename UnaryPredicate>
constexpr InputIt find_if_not(InputIt first, InputIt last, UnaryPredicate p)
{
	for (/**/; first != last; ++first) {
		if (!p(*first)) {
			return first;
		}
	}
	return last;
}

template<typename ForwardIt, typename UnaryPredicate>
constexpr ForwardIt partition(ForwardIt first, ForwardIt last, UnaryPredicate p)
{
	first = cstd::find_if_not(first, last, p);
	if (first == last) return first;

	for (ForwardIt i = first + 1; i != last; ++i) {
		if (p(*i)) {
			cstd::iter_swap(i, first);
			++first;
		}
	}
	return first;
}

// Workaround for lack of C++17 constexpr-lambda-functions.
template<typename T, typename Compare>
struct CmpLeft
{
	T pivot;
	Compare cmp;

	constexpr bool operator()(const T& elem) const {
		return cmp(elem, pivot);
	}
};

// Workaround for lack of C++17 constexpr-lambda-functions.
template<typename T, typename Compare>
struct CmpRight
{
	T pivot;
	Compare cmp;

	constexpr bool operator()(const T& elem) const {
		return !cmp(pivot, elem);
	}
};

// Textbook implementation of quick sort. Not optimized, but the intention is
// that it only gets executed during compile-time.
template<typename RAIt, typename Compare = std::less<>>
constexpr void sort(RAIt first, RAIt last, Compare cmp = Compare{})
{
	auto const N = last - first;
	if (N <= 1) return;
	auto const pivot = *(first + N / 2);
	auto const middle1 = cstd::partition(
	        first, last, CmpLeft<decltype(pivot), Compare>{pivot, cmp});
	auto const middle2 = cstd::partition(
	        middle1, last, CmpRight<decltype(pivot), Compare>{pivot, cmp});
	cstd::sort(first, middle1, cmp);
	cstd::sort(middle2, last, cmp);
}

template<typename InputIt1, typename InputIt2>
constexpr bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
                                       InputIt2 first2, InputIt2 last2)
{
	for (/**/; (first1 != last1) && (first2 != last2); ++first1, ++first2) {
		if (*first1 < *first2) return true;
		if (*first2 < *first1) return false;
	}
	return (first1 == last1) && (first2 != last2);
}

// In C++17 this is (constexpr) available as std::char_traits::length().
constexpr size_t strlen(const char* s) noexcept
{
	auto* p = s;
	while (*p) ++p;
	return p - s;
}


//
// Constrexpr reimplementation of (a subset of) std::array.
// Can be removed once we switch to C++17.
//

template<typename T, size_t N> struct array
{
	T storage[N];

	using value_type             = T;
	using pointer                = T*;
	using const_pointer          = const T*;
	using reference              = T&;
	using const_reference        = const T&;
	using iterator               = T*;
	using const_iterator         = const T*;
	using size_type              = size_t;
	using difference_type        = ptrdiff_t;

	constexpr T* begin() noexcept
	{
		return storage;
	}

	constexpr const T* begin() const noexcept
	{
		return storage;
	}

	constexpr T* end() noexcept
	{
		return storage + N;
	}

	constexpr const T* end() const noexcept
	{
		return storage + N;
	}

	constexpr size_type size() const noexcept
	{
		return N;
	}

	constexpr bool empty() const noexcept
	{
		return N == 0;
	}

	constexpr T& operator[](size_type n) noexcept
	{
		return storage[n];
	}

	constexpr const T& operator[](size_type n) const noexcept
	{
		return storage[n];
	}

	constexpr T& front() noexcept
	{
		return storage[0];
	}

	constexpr const T& front() const noexcept
	{
		return storage[0];
	}

	constexpr T& back() noexcept
	{
		return storage[N - 1];
	}

	constexpr const T& back() const noexcept
	{
		return storage[N - 1];
	}

	constexpr T* data() noexcept
	{
		return storage;
	}

	constexpr const T* data() const noexcept
	{
		return storage;
	}
};

template<typename V, typename ...Ts>
constexpr cstd::array<V, sizeof...(Ts)> array_of(Ts&& ...ts)
{
    return {{ std::forward<Ts>(ts)... }};
}


//
// Reimplementation of (a very small subset of) C++17 std::string_view.
//
// The main difference with our string_ref class is that cstd::string offers
// a constexpr constructor.
//

class string
{
public:
	constexpr string(const char* s)
		: dat(s), sz(cstd::strlen(s))
	{
	}

	constexpr const char* begin() const
	{
		return dat;
	}

	constexpr const char* end() const
	{
		return dat + sz;
	}

	friend constexpr bool operator<(string x, string y)
	{
		return cstd::lexicographical_compare(x.begin(), x.end(),
		                                     y.begin(), y.end());
	}

	operator string_ref() const
	{
		return string_ref(dat, sz);
	}

private:
	const char* dat;
	size_t sz;
};

#endif

// Reimplementation of various mathematical functions. You must specify an
// iteration count, this controls how accurate the result will be.
#if (__cplusplus < 201402) || (defined(__GNUC__) && !defined(__clang__))

// Ignore the iteration template arguments and call the standard function.
// We do this when:
// - In c++11 mode when we evaluate at run-time.
// - When using gcc which has constexpr versions of most mathematical functions
//   (this is a non-standard extension).
template<int>      CONSTEXPR double sin  (double x) { return std::sin  (x); }
template<int>      CONSTEXPR double cos  (double x) { return std::cos  (x); }
template<int, int> CONSTEXPR double log  (double x) { return std::log  (x); }
template<int, int> CONSTEXPR double log2 (double x) { return    ::log  (x) / ::log(2); } // should be std::log2(x) but this doesn't seem to compile in g++-4.8/g++-4.9 (bug?)
template<int, int> CONSTEXPR double log10(double x) { return std::log10(x); }
template<int>      CONSTEXPR double exp  (double x) { return std::exp  (x); }
template<int>      CONSTEXPR double exp2 (double x) { return    ::exp2 (x); } // see log2, but apparently no need to use exp(log(2) * x) here?!
template<int, int> CONSTEXPR double pow(double x, double y) { return std::pow(x, y); }
inline CONSTEXPR double round(double x) { return ::round(x); } // should be std::round(), see above

#else

constexpr double upow(double x, unsigned u)
{
    double y = 1.0;
    while (u) {
        if (u & 1) y *= x;
        x *= x;
        u >>= 1;
    }
    return y;
}

constexpr double ipow(double x, int i)
{
	return (i >= 0) ? upow(x, i) : upow(x, -i);
}

template<int ITERATIONS>
constexpr double exp(double x)
{
    // Split x into integral and fractional part:
    //   exp(x) = exp(i + f) = exp(i) * exp(f)
    //   with: i an int     (undefined if out of range)
    //         -1 < f < 1
    int i = int(x);
    double f = x - i;

    // Approximate exp(f) with Taylor series.
    double y = 1.0;
    double t = f;
    double n = 1.0;
    for (int k = 2; k < (2 + ITERATIONS); ++k) {
        y += t / n;
        t *= f;
        n *= k;
    }

    // Approximate exp(i) by squaring.
    int p = (i >= 0) ? i : -i; // abs(i);
    double s = upow(M_E, p);

    // Combine the results.
    if (i >= 0) {
        return y * s;
    } else {
        return y / s;
    }
}

constexpr double simple_fmod(double x, double y)
{
    assert(y > 0.0);
    return x - int(x / y) * y;
}

template<int ITERATIONS>
constexpr double sin_iter(double x)
{
    double x2 = x * x;
    double y = 0.0;
    double t = x;
    double n = 1.0;
    for (int k = 1; k < (1 + 4 * ITERATIONS); /**/) {
        y += t / n;
        t *= x2;
        n *= ++k;
        n *= ++k;

        y -= t / n;
        t *= x2;
        n *= ++k;
        n *= ++k;
    }
    return y;
}

template<int ITERATIONS>
constexpr double cos_iter(double x)
{
    double x2 = x * x;
    double y = 1.0;
    double t = x2;
    double n = 2.0;
    for (int k = 2; k < (2 + 4 * ITERATIONS); /**/) {
        y -= t / n;
        t *= x2;
        n *= ++k;
        n *= ++k;

        y += t / n;
        t *= x2;
        n *= ++k;
        n *= ++k;
    }
    return y;
}

template<int ITERATIONS>
constexpr double sin(double x)
{
    double sign = 1.0;

    // reduce to [0, +inf)
    if (x < 0.0) {
        sign = -1.0;
        x = -x;
    }

    // reduce to [0, 2pi)
    x = simple_fmod(x, 2 * M_PI);

    // reduce to [0, pi]
    if (x > M_PI) {
        sign = -sign;
        x -= M_PI;
    }

    // reduce to [0, pi/2]
    if (x > M_PI/2) {
        x = M_PI - x;
    }

    // reduce to [0, pi/4]
    if (x > M_PI/4) {
        x = M_PI/2 - x;
        return sign * cos_iter<ITERATIONS>(x);
    } else {
        return sign * sin_iter<ITERATIONS>(x);
    }
}

template<int ITERATIONS>
constexpr double cos(double x)
{
    double sign = 1.0;

    // reduce to [0, +inf)
    if (x < 0.0) {
        x = -x;
    }

    // reduce to [0, 2pi)
    x = simple_fmod(x, 2 * M_PI);

    // reduce to [0, pi]
    if (x > M_PI) {
        x = 2.0 * M_PI - x;
    }

    // reduce to [0, pi/2]
    if (x > M_PI/2) {
        sign = -sign;
        x = M_PI - x;
    }

    // reduce to [0, pi/4]
    if (x > M_PI/4) {
        x = M_PI/2 - x;
        return sign * sin_iter<ITERATIONS>(x);
    } else {
        return sign * cos_iter<ITERATIONS>(x);
    }
}


// https://en.wikipedia.org/wiki/Natural_logarithm#High_precision
template<int E_ITERATIONS, int L_ITERATIONS>
constexpr double log(double x)
{
	int a = 0;
	while (x <= 0.25) {
		x *= M_E;
		++a;
	}
	double y = 0.0;
	for (int i = 0; i < L_ITERATIONS; ++i) {
		auto ey = cstd::exp<E_ITERATIONS>(y);
		y = y + 2.0 * (x - ey) / (x + ey);
	}
	return y - a;
}

template<int E_ITERATIONS, int L_ITERATIONS>
constexpr double log2(double x)
{
	return cstd::log<E_ITERATIONS, L_ITERATIONS>(x) / M_LN2;
}

template<int E_ITERATIONS, int L_ITERATIONS>
constexpr double log10(double x)
{
	return cstd::log<E_ITERATIONS, L_ITERATIONS>(x) / M_LN10;
}

template<int E_ITERATIONS, int L_ITERATIONS>
constexpr double pow(double x, double y)
{
	return cstd::exp<E_ITERATIONS>(cstd::log<E_ITERATIONS, L_ITERATIONS>(x) * y);
}

template<int ITERATIONS>
constexpr double exp2(double x)
{
	return cstd::exp<ITERATIONS>(M_LN2 * x);
}

constexpr double round(double x)
{
	return (x >= 0) ?  int( x + 0.5)
	                : -int(-x + 0.5);
}

#endif

} // namespace cstd

#endif
