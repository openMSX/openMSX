#ifndef CSTD_HH
#define CSTD_HH

#include "Math.hh"
#include "xrange.hh"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string_view>
#include <utility>

namespace cstd {

template<typename T>
[[nodiscard]] constexpr T abs(T t)
{
    return (t >= 0) ? t : -t;
}

// Reimplementation of various mathematical functions. You must specify an
// iteration count, this controls how accurate the result will be.
#if (defined(__GNUC__) && !defined(__clang__))

// Gcc has constexpr versions of most mathematical functions (this is a
// non-standard extension). Prefer those over our implementations.
template<int>      [[nodiscard]] constexpr double sin  (double x) { return std::sin  (x); }
template<int>      [[nodiscard]] constexpr double cos  (double x) { return std::cos  (x); }
template<int, int> [[nodiscard]] constexpr double log  (double x) { return std::log  (x); }
template<int, int> [[nodiscard]] constexpr double log2 (double x) { return    ::log  (x) / ::log(2); } // should be std::log2(x) but this doesn't seem to compile in g++-4.8/g++-4.9 (bug?)
template<int, int> [[nodiscard]] constexpr double log10(double x) { return std::log10(x); }
template<int>      [[nodiscard]] constexpr double exp  (double x) { return std::exp  (x); }
template<int>      [[nodiscard]] constexpr double exp2 (double x) { return    ::exp2 (x); } // see log2, but apparently no need to use exp(log(2) * x) here?!
template<int, int> [[nodiscard]] constexpr double pow(double x, double y) { return std::pow(x, y); }
[[nodiscard]] constexpr double round(double x) { return ::round(x); } // should be std::round(), see above
[[nodiscard]] constexpr float  round(float  x) { return ::round(x); }
[[nodiscard]] constexpr double sqrt (double x) { return ::sqrt (x); }

#else

[[nodiscard]] constexpr double upow(double x, unsigned u)
{
	double y = 1.0;
	while (u) {
		if (u & 1) y *= x;
		x *= x;
		u >>= 1;
	}
	return y;
}

[[nodiscard]] constexpr double ipow(double x, int i)
{
	return (i >= 0) ? upow(x, i) : upow(x, -i);
}

template<int ITERATIONS>
[[nodiscard]] constexpr double exp(double x)
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
	for (auto k : xrange(ITERATIONS)) {
		y += t / n;
		t *= f;
		n *= k + 2;
	}

	// Approximate exp(i) by squaring.
	int p = (i >= 0) ? i : -i; // abs(i);
	double s = upow(Math::e, p);

	// Combine the results.
	if (i >= 0) {
		return y * s;
	} else {
		return y / s;
	}
}

[[nodiscard]] constexpr double simple_fmod(double x, double y)
{
	assert(y > 0.0);
	return x - int(x / y) * y;
}

template<int ITERATIONS>
[[nodiscard]] constexpr double sin_iter(double x)
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
[[nodiscard]] constexpr double cos_iter(double x)
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
[[nodiscard]] constexpr double sin(double x)
{
	double sign = 1.0;

	// reduce to [0, +inf)
	if (x < 0.0) {
		sign = -1.0;
		x = -x;
	}

	// reduce to [0, 2pi)
	x = simple_fmod(x, 2 * Math::pi);

	// reduce to [0, pi]
	if (x > Math::pi) {
		sign = -sign;
		x -= Math::pi;
	}

	// reduce to [0, pi/2]
	if (x > Math::pi / 2) {
		x = Math::pi - x;
	}

	// reduce to [0, pi/4]
	if (x > Math::pi / 4) {
		x = Math::pi / 2 - x;
		return sign * cos_iter<ITERATIONS>(x);
	} else {
		return sign * sin_iter<ITERATIONS>(x);
	}
}

template<int ITERATIONS>
[[nodiscard]] constexpr double cos(double x)
{
	double sign = 1.0;

	// reduce to [0, +inf)
	if (x < 0.0) {
		x = -x;
	}

	// reduce to [0, 2pi)
	x = simple_fmod(x, 2 * Math::pi);

	// reduce to [0, pi]
	if (x > Math::pi) {
		x = 2.0 * Math::pi - x;
	}

	// reduce to [0, pi/2]
	if (x > Math::pi / 2) {
		sign = -sign;
		x = Math::pi - x;
	}

	// reduce to [0, pi/4]
	if (x > Math::pi / 4) {
		x = Math::pi / 2 - x;
		return sign * sin_iter<ITERATIONS>(x);
	} else {
		return sign * cos_iter<ITERATIONS>(x);
	}
}


// https://en.wikipedia.org/wiki/Natural_logarithm#High_precision
template<int E_ITERATIONS, int L_ITERATIONS>
[[nodiscard]] constexpr double log(double x)
{
	int a = 0;
	while (x <= 0.25) {
		x *= Math::e;
		++a;
	}
	double y = 0.0;
	repeat(L_ITERATIONS, [&] {
		auto ey = cstd::exp<E_ITERATIONS>(y);
		y = y + 2.0 * (x - ey) / (x + ey);
	});
	return y - a;
}

template<int E_ITERATIONS, int L_ITERATIONS>
[[nodiscard]] constexpr double log2(double x)
{
	return cstd::log<E_ITERATIONS, L_ITERATIONS>(x) / Math::ln2;
}

template<int E_ITERATIONS, int L_ITERATIONS>
[[nodiscard]] constexpr double log10(double x)
{
	return cstd::log<E_ITERATIONS, L_ITERATIONS>(x) / Math::ln10;
}

template<int E_ITERATIONS, int L_ITERATIONS>
[[nodiscard]] constexpr double pow(double x, double y)
{
	return cstd::exp<E_ITERATIONS>(cstd::log<E_ITERATIONS, L_ITERATIONS>(x) * y);
}

template<int ITERATIONS>
[[nodiscard]] constexpr double exp2(double x)
{
	return cstd::exp<ITERATIONS>(Math::ln2 * x);
}

[[nodiscard]] constexpr double round(double x)
{
	return (x >= 0) ?  int( x + 0.5)
	                : -int(-x + 0.5);
}

[[nodiscard]] constexpr float round(float x)
{
	return (x >= 0) ?  int( x + 0.5f)
	                : -int(-x + 0.5f);
}

[[nodiscard]] constexpr double sqrt(double x)
{
    assert(x >= 0.0);
    double curr = x;
    double prev = 0.0;
    while (curr != prev) {
        prev = curr;
        curr = 0.5 * (curr + x / curr);
    }
    return curr;
}

#endif

} // namespace cstd

#endif
