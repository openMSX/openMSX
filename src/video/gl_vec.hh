#ifndef GL_VEC_HH
#define GL_VEC_HH

// This code implements a mathematical vector, comparable in functionality
// and syntax to the vector types in GLSL.
//
// Only vector sizes 2, 3 and 4 are supported. Though when it doesn't
// complicate stuff the code was written to support any size.
//
// Most basic functionality is already there, but this is not meant to be a
// full set of GLSL functions. We can always extend the functionality if/when
// required.
//
// In the past we had (some) manual SSE optimizations in this code. Though for
// the functions that matter (matrix-vector and matrix-matrix multiplication are
// built on top of the functions in this file), the compiler's
// auto-vectorization has become as good as the manually vectorized code.

#include "Math.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

namespace gl {

// Vector with N components of type T.
template<int N, typename T> class vecN
{
public:
	// Default copy-constructor and assignment operator.

	// Construct vector containing all zeros.
	constexpr vecN()
	{
		for (auto i : xrange(N)) e[i] = T(0);
	}

	// Construct vector containing the same value repeated N times.
	constexpr explicit vecN(T x)
	{
		for (auto i : xrange(N)) e[i] = x;
	}

	// Conversion constructor from vector of same size but different type
	template<typename T2>
	constexpr explicit vecN(const vecN<N, T2>& x)
	{
		for (auto i : xrange(N)) e[i] = T(x[i]);
	}

	// Construct from larger vector (higher order elements are dropped).
	template<int N2> constexpr explicit vecN(const vecN<N2, T>& x)
	{
		static_assert(N2 > N, "wrong vector length in constructor");
		for (auto i : xrange(N)) e[i] = x[i];
	}

	// Construct vector from 2 given values (only valid when N == 2).
	constexpr vecN(T x, T y)
		: e{x, y}
	{
		static_assert(N == 2, "wrong #constructor arguments");
	}

	// Construct vector from 3 given values (only valid when N == 3).
	constexpr vecN(T x, T y, T z)
		: e{x, y, z}
	{
		static_assert(N == 3, "wrong #constructor arguments");
	}

	// Construct vector from 4 given values (only valid when N == 4).
	constexpr vecN(T x, T y, T z, T w)
		: e{x, y, z, w}
	{
		static_assert(N == 4, "wrong #constructor arguments");
	}

	// Construct vector from concatenating a scalar and a (smaller) vector.
	template<int N2>
	constexpr vecN(T x, const vecN<N2, T>& y)
	{
		static_assert((1 + N2) == N, "wrong vector length in constructor");
		e[0] = x;
		for (auto i : xrange(N2)) e[i + 1] = y[i];
	}

	// Construct vector from concatenating a (smaller) vector and a scalar.
	template<int N1>
	constexpr vecN(const vecN<N1, T>& x, T y)
	{
		static_assert((N1 + 1) == N, "wrong vector length in constructor");
		for (auto i : xrange(N1)) e[i] = x[i];
		e[N1] = y;
	}

	// Construct vector from concatenating two (smaller) vectors.
	template<int N1, int N2>
	constexpr vecN(const vecN<N1, T>& x, const vecN<N2, T>& y)
	{
		static_assert((N1 + N2) == N, "wrong vector length in constructor");
		for (auto i : xrange(N1)) e[i     ] = x[i];
		for (auto i : xrange(N2)) e[i + N1] = y[i];
	}

	// Access the i-th element of this vector.
	[[nodiscard]] constexpr T  operator[](unsigned i) const {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return e[i];
	}
	[[nodiscard]] constexpr T& operator[](unsigned i) {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return e[i];
	}

	// For structured bindings
	template<size_t I> [[nodiscard]] constexpr T  get() const noexcept { return (*this)[I]; }
	template<size_t I> [[nodiscard]] constexpr T& get()       noexcept { return (*this)[I]; }

	// Assignment version of the +,-,* operations defined below.
	constexpr vecN& operator+=(const vecN& x) { *this = *this + x; return *this; }
	constexpr vecN& operator-=(const vecN& x) { *this = *this - x; return *this; }
	constexpr vecN& operator*=(const vecN& x) { *this = *this * x; return *this; }
	constexpr vecN& operator*=(T           x) { *this = *this * x; return *this; }

private:
	T e[N];
};


// Convenience typedefs (same names as used by GLSL).
using  vec2 = vecN<2, float>;
using  vec3 = vecN<3, float>;
using  vec4 = vecN<4, float>;
using ivec2 = vecN<2, int>;
using ivec3 = vecN<3, int>;
using ivec4 = vecN<4, int>;


// -- Scalar functions --

// reciprocal square root
[[nodiscard]] inline float rsqrt(float x)
{
	return 1.0f / sqrtf(x);
}
[[nodiscard]] inline double rsqrt(double x)
{
	return 1.0 / sqrt(x);
}

// convert radians <-> degrees
template<typename T> [[nodiscard]] constexpr T radians(T d)
{
	return d * T(M_PI / 180.0);
}
template<typename T> [[nodiscard]] constexpr T degrees(T r)
{
	return r * T(180.0 / M_PI);
}


// -- Vector functions --

// vector equality / inequality
template<int N, typename T>
[[nodiscard]] constexpr bool operator==(const vecN<N, T>& x, const vecN<N, T>& y)
{
	for (auto i : xrange(N)) if (x[i] != y[i]) return false;
	return true;
}

// vector negation
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator-(const vecN<N, T>& x)
{
	return vecN<N, T>() - x;
}

// vector + vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator+(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = x[i] + y[i];
	return r;
}

// vector - vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator-(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = x[i] - y[i];
	return r;
}

// scalar * vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator*(T x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = x * y[i];
	return r;
}

// vector * scalar
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator*(const vecN<N, T>& x, T y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = x[i] * y;
	return r;
}

// vector * vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator*(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = x[i] * y[i];
	return r;
}

// element-wise reciprocal
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> recip(const vecN<N, T>& x)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = T(1) / x[i];
	return r;
}

// scalar / vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator/(T x, const vecN<N, T>& y)
{
	return x * recip(y);
}

// vector / scalar
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator/(const vecN<N, T>& x, T y)
{
	return x * (T(1) / y);
}

// vector / vector
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator/(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return x * recip(y);
}

// min(vector, vector)
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> min(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = std::min(x[i], y[i]);
	return r;
}

// min(vector, vector)
template<int N, typename T>
[[nodiscard]] constexpr T min_component(const vecN<N, T>& x)
{
	T r = x[0];
	for (auto i : xrange(1, N)) r = std::min(r, x[i]);
	return r;
}

// max(vector, vector)
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> max(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (auto i : xrange(N)) r[i] = std::max(x[i], y[i]);
	return r;
}

// clamp(vector, vector, vector)
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> clamp(const vecN<N, T>& x, const vecN<N, T>& minVal, const vecN<N, T>& maxVal)
{
	return min(maxVal, max(minVal, x));
}

// clamp(vector, scalar, scalar)
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> clamp(const vecN<N, T>& x, T minVal, T maxVal)
{
	return clamp(x, vecN<N, T>(minVal), vecN<N, T>(maxVal));
}

// sum of components
template<int N, typename T>
[[nodiscard]] constexpr T sum(const vecN<N, T>& x)
{
	T result(0);
	for (auto i : xrange(N)) result += x[i];
	return result;
}
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> sum_broadcast(const vecN<N, T>& x)
{
	return vecN<N, T>(sum(x));
}

// dot product
template<int N, typename T>
[[nodiscard]] constexpr T dot(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return sum(x * y);
}
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> dot_broadcast(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return sum_broadcast(x * y);
}

// squared length (norm-2)
template<int N, typename T>
[[nodiscard]] constexpr T length2(const vecN<N, T>& x)
{
	return dot(x, x);
}

// length (norm-2)
template<int N, typename T>
[[nodiscard]] inline T length(const vecN<N, T>& x)
{
	return sqrt(length2(x));
}

// normalize vector
template<int N, typename T>
[[nodiscard]] inline vecN<N, T> normalize(const vecN<N, T>& x)
{
	return x * rsqrt(length2(x));
}

// cross product (only defined for vectors of length 3)
template<typename T>
[[nodiscard]] constexpr vecN<3, T> cross(const vecN<3, T>& x, const vecN<3, T>& y)
{
	return vecN<3, T>(x[1] * y[2] - x[2] * y[1],
	                  x[2] * y[0] - x[0] * y[2],
	                  x[0] * y[1] - x[1] * y[0]);
}

// round each component to the nearest integer (returns a vector of integers)
template<int N, typename T>
[[nodiscard]] inline vecN<N, int> round(const vecN<N, T>& x)
{
	vecN<N, int> r;
	// note: std::lrint() is more generic (e.g. also works with double),
	// but Dingux doesn't seem to have std::lrint().
	for (auto i : xrange(N)) r[i] = lrintf(x[i]);
	return r;
}

// truncate each component to the nearest integer that is not bigger in
// absolute value (returns a vector of integers)
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, int> trunc(const vecN<N, T>& x)
{
	vecN<N, int> r;
	for (auto i : xrange(N)) r[i] = int(x[i]);
	return r;
}

// Textual representation. (Only) used to debug unittest.
template<int N, typename T>
std::ostream& operator<<(std::ostream& os, const vecN<N, T>& x)
{
	os << "[ ";
	for (auto i : xrange(N)) {
		os << x[i] << ' ';
	}
	os << ']';
	return os;
}

} // namespace gl

// Support for structured bindings
namespace std {
// On some platforms tuple_size is a class and on others it is a struct.
// Such a mismatch is only a problem when targeting the Microsoft C++ ABI,
// which we don't do when compiling with Clang.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
	template<int N, typename T> class tuple_size<gl::vecN<N, T>>
		: public std::integral_constant<size_t, N> {};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
	template<size_t I, int N, typename T> class tuple_element<I, gl::vecN<N, T>> {
	public:
		using type = T;
	};
}

#endif // GL_VEC_HH
