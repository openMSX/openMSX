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
#include "narrow.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>

namespace gl {

// Vector with N components of type T.
template<int N, typename T> class vecN;

// Specialization for N=2.
template<typename T> class vecN<2, T>
{
public:
	// Default copy-constructor and assignment operator.

	// Construct vector containing all zeros.
	constexpr vecN() : x(T(0)), y(T(0)) {}

	// Construct vector containing the same value repeated N times.
	constexpr explicit vecN(T t) : x(t), y(t) {}

	// Conversion constructor from vector of same size but different type.
	template<typename T2>
	constexpr explicit vecN(const vecN<2, T2>& v) : x(T(v.x)), y(T(v.y)) {}

	// Construct from larger vector (higher order elements are dropped).
	template<int N2> constexpr explicit vecN(const vecN<N2, T>& v) : x(v.x), y(v.y) {}

	// Construct vector from 2 given values.
	constexpr vecN(T a, T b) : x(a), y(b) {}

	// Access the i-th element of this vector.
	[[nodiscard]] constexpr T  operator[](unsigned i) const {
		if (i == 0) return x;
		if (i == 1) return y;
		UNREACHABLE;
	}
	[[nodiscard]] constexpr T& operator[](unsigned i) {
		if (i == 0) return x;
		if (i == 1) return y;
		UNREACHABLE;
	}

	[[nodiscard]] constexpr const T* data() const { return &x; }
	[[nodiscard]] constexpr       T* data()       { return &x; }

	// For structured bindings
	template<size_t I> [[nodiscard]] constexpr T  get() const noexcept { return (*this)[I]; }
	template<size_t I> [[nodiscard]] constexpr T& get()       noexcept { return (*this)[I]; }

	// Assignment version of the +,-,* operations defined below.
	constexpr vecN& operator+=(const vecN& v) { *this = *this + v; return *this; }
	constexpr vecN& operator-=(const vecN& v) { *this = *this - v; return *this; }
	constexpr vecN& operator*=(const vecN& v) { *this = *this * v; return *this; }
	constexpr vecN& operator*=(T           t) { *this = *this * t; return *this; }

	[[nodiscard]] constexpr bool operator==(const vecN&) const = default;

	// vector + vector
	[[nodiscard]] constexpr friend vecN operator+(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(2)) r[i] = v1[i] + v2[i];
		return r;
	}

	// vector - vector
	[[nodiscard]] constexpr friend vecN operator-(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(2)) r[i] = v1[i] - v2[i];
		return r;
	}

	// scalar * vector
	[[nodiscard]] constexpr friend vecN operator*(T a, const vecN& v) {
		vecN r;
		for (auto i : xrange(2)) r[i] = a * v[i];
		return r;
	}

	// vector * scalar
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v, T a) {
		vecN r;
		for (auto i : xrange(2)) r[i] = v[i] * a;
		return r;
	}

	// vector * vector
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v1, const vecN& v2)
	{
		vecN r;
		for (auto i : xrange(2)) r[i] = v1[i] * v2[i];
		return r;
	}

	// element-wise reciprocal
	[[nodiscard]] constexpr friend vecN recip(const vecN& v) {
		vecN r;
		for (auto i : xrange(2)) r[i] = T(1) / v[i];
		return r;
	}

	// scalar / vector
	[[nodiscard]] constexpr friend vecN operator/(T a, const vecN& v) {
		return a * recip(v);
	}

	// vector / scalar
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v, T a) {
		return v * (T(1) / a);
	}

	// vector / vector
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v1, const vecN& v2) {
		return v1 * recip(v2);
	}

	// Textual representation. (Only) used to debug unittest.
	friend std::ostream& operator<<(std::ostream& os, const vecN& v) {
		os << "[ ";
		for (auto i : xrange(2)) {
			os << v[i] << ' ';
		}
		os << ']';
		return os;
	}

public:
	T x, y;
};

// Specialization for N=3.
template<typename T> class vecN<3, T>
{
public:
	constexpr vecN() : x(T(0)), y(T(0)), z(T(0)) {}
	constexpr explicit vecN(T t) : x(t), y(t), z(t) {}
	template<typename T2>
	constexpr explicit vecN(const vecN<3, T2>& v) : x(T(v.x)), y(T(v.y)), z(T(v.z)) {}
	constexpr explicit vecN(const vecN<4, T>& v) : x(v.x), y(v.y), z(v.z) {}
	constexpr vecN(T a, T b, T c) : x(a), y(b), z(c) {}
	constexpr vecN(T a, const vecN<2, T>& b) : x(a), y(b.x), z(b.y) {}
	constexpr vecN(const vecN<2, T>& a, T b) : x(a.x), y(a.y), z(b) {}

	[[nodiscard]] constexpr T  operator[](unsigned i) const {
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		UNREACHABLE;
	}
	[[nodiscard]] constexpr T& operator[](unsigned i) {
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		UNREACHABLE;
	}

	[[nodiscard]] constexpr const T* data() const { return &x; }
	[[nodiscard]] constexpr       T* data()       { return &x; }

	template<size_t I> [[nodiscard]] constexpr T  get() const noexcept { return (*this)[I]; }
	template<size_t I> [[nodiscard]] constexpr T& get()       noexcept { return (*this)[I]; }

	constexpr vecN& operator+=(const vecN& v) { *this = *this + v; return *this; }
	constexpr vecN& operator-=(const vecN& v) { *this = *this - v; return *this; }
	constexpr vecN& operator*=(const vecN& v) { *this = *this * v; return *this; }
	constexpr vecN& operator*=(T           t) { *this = *this * t; return *this; }

	[[nodiscard]] constexpr bool operator==(const vecN&) const = default;

	// vector + vector
	[[nodiscard]] constexpr friend vecN operator+(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(3)) r[i] = v1[i] + v2[i];
		return r;
	}

	// vector - vector
	[[nodiscard]] constexpr friend vecN operator-(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(3)) r[i] = v1[i] - v2[i];
		return r;
	}

	// scalar * vector
	[[nodiscard]] constexpr friend vecN operator*(T a, const vecN& v) {
		vecN r;
		for (auto i : xrange(3)) r[i] = a * v[i];
		return r;
	}

	// vector * scalar
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v, T a) {
		vecN r;
		for (auto i : xrange(3)) r[i] = v[i] * a;
		return r;
	}

	// vector * vector
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v1, const vecN& v2)
	{
		vecN r;
		for (auto i : xrange(3)) r[i] = v1[i] * v2[i];
		return r;
	}

	// element-wise reciprocal
	[[nodiscard]] constexpr friend vecN recip(const vecN& v) {
		vecN r;
		for (auto i : xrange(3)) r[i] = T(1) / v[i];
		return r;
	}

	// scalar / vector
	[[nodiscard]] constexpr friend vecN operator/(T a, const vecN& v) {
		return a * recip(v);
	}

	// vector / scalar
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v, T a) {
		return v * (T(1) / a);
	}

	// vector / vector
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v1, const vecN& v2) {
		return v1 * recip(v2);
	}

	// Textual representation. (Only) used to debug unittest.
	friend std::ostream& operator<<(std::ostream& os, const vecN& v) {
		os << "[ ";
		for (auto i : xrange(3)) {
			os << v[i] << ' ';
		}
		os << ']';
		return os;
	}

public:
	T x, y, z;
};

// Specialization for N=4.
template<typename T> class vecN<4, T>
{
public:
	constexpr vecN() : x(T(0)), y(T(0)), z(T(0)), w(T(0)) {}
	constexpr explicit vecN(T t) : x(t), y(t), z(t), w(t) {}
	template<typename T2>
	constexpr explicit vecN(const vecN<4, T2>& v) : x(T(v.x)), y(T(v.y)), z(T(v.z)), w(T(v.w)) {}
	constexpr vecN(T a, T b, T c, T d) : x(a), y(b), z(c), w(d) {}
	constexpr vecN(T a, const vecN<3, T>& b) : x(a), y(b.x), z(b.y), w(b.z) {}
	constexpr vecN(const vecN<3, T>& a, T b) : x(a.x), y(a.y), z(a.z), w(b) {}
	constexpr vecN(const vecN<2, T>& a, const vecN<2, T>& b) : x(a.x), y(a.y), z(b.x), w(b.y) {}

	[[nodiscard]] constexpr T  operator[](unsigned i) const {
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		if (i == 3) return w;
		UNREACHABLE;
	}
	[[nodiscard]] constexpr T& operator[](unsigned i) {
		if (i == 0) return x;
		if (i == 1) return y;
		if (i == 2) return z;
		if (i == 3) return w;
		UNREACHABLE;
	}

	[[nodiscard]] constexpr const T* data() const { return &x; }
	[[nodiscard]] constexpr       T* data()       { return &x; }

	template<size_t I> [[nodiscard]] constexpr T  get() const noexcept { return (*this)[I]; }
	template<size_t I> [[nodiscard]] constexpr T& get()       noexcept { return (*this)[I]; }

	// Assignment version of the +,-,* operations defined below.
	constexpr vecN& operator+=(const vecN& v) { *this = *this + v; return *this; }
	constexpr vecN& operator-=(const vecN& v) { *this = *this - v; return *this; }
	constexpr vecN& operator*=(const vecN& v) { *this = *this * v; return *this; }
	constexpr vecN& operator*=(T           t) { *this = *this * t; return *this; }

	[[nodiscard]] constexpr bool operator==(const vecN&) const = default;

	// vector + vector
	[[nodiscard]] constexpr friend vecN operator+(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(4)) r[i] = v1[i] + v2[i];
		return r;
	}

	// vector - vector
	[[nodiscard]] constexpr friend vecN operator-(const vecN& v1, const vecN& v2) {
		vecN r;
		for (auto i : xrange(4)) r[i] = v1[i] - v2[i];
		return r;
	}

	// scalar * vector
	[[nodiscard]] constexpr friend vecN operator*(T a, const vecN& v) {
		vecN r;
		for (auto i : xrange(4)) r[i] = a * v[i];
		return r;
	}

	// vector * scalar
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v, T a) {
		vecN r;
		for (auto i : xrange(4)) r[i] = v[i] * a;
		return r;
	}

	// vector * vector
	[[nodiscard]] constexpr friend vecN operator*(const vecN& v1, const vecN& v2)
	{
		vecN r;
		for (auto i : xrange(4)) r[i] = v1[i] * v2[i];
		return r;
	}

	// element-wise reciprocal
	[[nodiscard]] constexpr friend vecN recip(const vecN& v) {
		vecN r;
		for (auto i : xrange(4)) r[i] = T(1) / v[i];
		return r;
	}

	// scalar / vector
	[[nodiscard]] constexpr friend vecN operator/(T a, const vecN& v) {
		return a * recip(v);
	}

	// vector / scalar
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v, T a) {
		return v * (T(1) / a);
	}

	// vector / vector
	[[nodiscard]] constexpr friend vecN operator/(const vecN& v1, const vecN& v2) {
		return v1 * recip(v2);
	}

	// Textual representation. (Only) used to debug unittest.
	friend std::ostream& operator<<(std::ostream& os, const vecN& v) {
		os << "[ ";
		for (auto i : xrange(4)) {
			os << v[i] << ' ';
		}
		os << ']';
		return os;
	}

public:
	T x, y, z, w;
};


// Convenience typedefs (same names as used by GLSL).
using  vec2 = vecN<2, float>;
using  vec3 = vecN<3, float>;
using  vec4 = vecN<4, float>;
using ivec2 = vecN<2, int>;
using ivec3 = vecN<3, int>;
using ivec4 = vecN<4, int>;

static_assert(sizeof( vec2) == 2 * sizeof(float));
static_assert(sizeof( vec3) == 3 * sizeof(float));
static_assert(sizeof( vec4) == 4 * sizeof(float));
static_assert(sizeof(ivec2) == 2 * sizeof(int));
static_assert(sizeof(ivec3) == 3 * sizeof(int));
static_assert(sizeof(ivec4) == 4 * sizeof(int));


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
	return d * T(Math::pi / 180.0);
}
template<typename T> [[nodiscard]] constexpr T degrees(T r)
{
	return r * T(180.0 / Math::pi);
}


// -- Vector functions --

// vector negation
template<int N, typename T>
[[nodiscard]] constexpr vecN<N, T> operator-(const vecN<N, T>& x)
{
	return vecN<N, T>() - x;
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
	return std::sqrt(length2(x));
}

// normalize vector
template<int N, typename T>
[[nodiscard]] inline vecN<N, T> normalize(const vecN<N, T>& x)
{
	return x * rsqrt(length2(x));
}

// cross product (only defined for vectors of length 3)
template<typename T>
[[nodiscard]] constexpr vecN<3, T> cross(const vecN<3, T>& a, const vecN<3, T>& b)
{
	return vecN<3, T>(a.y * b.z - a.z * b.y,
	                  a.z * b.x - a.x * b.z,
	                  a.x * b.y - a.y * b.x);
}

// round each component to the nearest integer (returns a vector of integers)
template<int N, typename T>
[[nodiscard]] inline vecN<N, int> round(const vecN<N, T>& x)
{
	vecN<N, int> r;
	// note: std::lrint() is more generic (e.g. also works with double),
	// but Dingux doesn't seem to have std::lrint().
	for (auto i : xrange(N)) r[i] = narrow_cast<int>(lrintf(narrow_cast<float>(x[i])));
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
