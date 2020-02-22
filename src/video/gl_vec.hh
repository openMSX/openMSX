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
// Only the 'common' operations on vec4 (4 float values) are 'well' optimized.
// I verified that they can be used to built a 4x4 matrix multiplication
// routine that is as efficient as a hand-written SSE assembly routine
// (verified with gcc-4.8 on x86_64). Actually the other routines are likely
// efficient as well, but I mean it wasn't the main goal for this code.
//
// Calling convention: vec4 internally takes up one 128-bit xmm register. It
// can be efficientlt passed by value. The other vector types are passed by
// const-reference. Though also in the latter case usually the compiler can
// optimize away the indirection.

#include "Math.hh"
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
	vecN()
	{
		for (int i = 0; i < N; ++i) e[i] = T(0);
	}

	// Construct vector containing the same value repeated N times.
	explicit vecN(T x)
	{
		for (int i = 0; i < N; ++i) e[i] = x;
	}

	// Conversion constructor from vector of same size but different type
	template<typename T2>
	explicit vecN(const vecN<N, T2>& x)
	{
		for (int i = 0; i < N; ++i) e[i] = T(x[i]);
	}

	// Construct from larger vector (higher order elements are dropped).
	template<int N2> explicit vecN(const vecN<N2, T>& x)
	{
		static_assert(N2 > N, "wrong vector length in constructor");
		for (int i = 0; i < N; ++i) e[i] = x[i];
	}

	// Construct vector from 2 given values (only valid when N == 2).
	vecN(T x, T y)
	{
		static_assert(N == 2, "wrong #constructor arguments");
		e[0] = x; e[1] = y;
	}

	// Construct vector from 3 given values (only valid when N == 3).
	vecN(T x, T y, T z)
	{
		static_assert(N == 3, "wrong #constructor arguments");
		e[0] = x; e[1] = y; e[2] = z;
	}

	// Construct vector from 4 given values (only valid when N == 4).
	vecN(T x, T y, T z, T w)
	{
		static_assert(N == 4, "wrong #constructor arguments");
		e[0] = x; e[1] = y; e[2] = z; e[3] = w;
	}

	// Construct vector from concatenating a scalar and a (smaller) vector.
	template<int N2>
	vecN(T x, const vecN<N2, T>& y)
	{
		static_assert((1 + N2) == N, "wrong vector length in constructor");
		e[0] = x;
		for (int i = 0; i < N2; ++i) e[i + 1] = y[i];
	}

	// Construct vector from concatenating a (smaller) vector and a scalar.
	template<int N1>
	vecN(const vecN<N1, T>& x, T y)
	{
		static_assert((N1 + 1) == N, "wrong vector length in constructor");
		for (int i = 0; i < N1; ++i) e[i] = x[i];
		e[N1] = y;
	}

	// Construct vector from concatenating two (smaller) vectors.
	template<int N1, int N2>
	vecN(const vecN<N1, T>& x, const vecN<N2, T>& y)
	{
		static_assert((N1 + N2) == N, "wrong vector length in constructor");
		for (int i = 0; i < N1; ++i) e[i     ] = x[i];
		for (int i = 0; i < N2; ++i) e[i + N1] = y[i];
	}

	// Access the i-th element of this vector.
	T  operator[](unsigned i) const {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return e[i];
	}
	T& operator[](unsigned i) {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return e[i];
	}

	// For structured bindings
	template<size_t I> T  get() const noexcept { return (*this)[I]; }
	template<size_t I> T& get()       noexcept { return (*this)[I]; }

	// Assignment version of the +,-,* operations defined below.
	vecN& operator+=(const vecN& x) { *this = *this + x; return *this; }
	vecN& operator-=(const vecN& x) { *this = *this - x; return *this; }
	vecN& operator*=(const vecN& x) { *this = *this * x; return *this; }
	vecN& operator*=(T           x) { *this = *this * x; return *this; }

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
inline float rsqrt(float x)
{
	return 1.0f / sqrtf(x);
}
inline double rsqrt(double x)
{
	return 1.0 / sqrt(x);
}

// convert radians <-> degrees
template<typename T> inline T radians(T d)
{
	return d * T(M_PI / 180.0);
}
template<typename T> inline T degrees(T r)
{
	return r * T(180.0 / M_PI);
}


// -- Vector functions --

// vector equality / inequality
template<int N, typename T>
inline bool operator==(const vecN<N, T>& x, const vecN<N, T>& y)
{
	for (int i = 0; i < N; ++i) if (x[i] != y[i]) return false;
	return true;
}
template<int N, typename T>
inline bool operator!=(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return !(x == y);
}

// vector negation
template<int N, typename T>
inline vecN<N, T> operator-(const vecN<N, T>& x)
{
	return vecN<N, T>() - x;
}

// vector + vector
template<int N, typename T>
inline vecN<N, T> operator+(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = x[i] + y[i];
	return r;
}

// vector - vector
template<int N, typename T>
inline vecN<N, T> operator-(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = x[i] - y[i];
	return r;
}

// scalar * vector
template<int N, typename T>
inline vecN<N, T> operator*(T x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = x * y[i];
	return r;
}

// vector * scalar
template<int N, typename T>
inline vecN<N, T> operator*(const vecN<N, T>& x, T y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = x[i] * y;
	return r;
}

// vector * vector
template<int N, typename T>
inline vecN<N, T> operator*(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = x[i] * y[i];
	return r;
}

// element-wise reciprocal
template<int N, typename T>
inline vecN<N, T> recip(const vecN<N, T>& x)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = T(1) / x[i];
	return r;
}

// scalar / vector
template<int N, typename T>
inline vecN<N, T> operator/(T x, const vecN<N, T>& y)
{
	return x * recip(y);
}

// vector / scalar
template<int N, typename T>
inline vecN<N, T> operator/(const vecN<N, T>& x, T y)
{
	return x * (T(1) / y);
}

// vector / vector
template<int N, typename T>
inline vecN<N, T> operator/(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return x * recip(y);
}

// min(vector, vector)
template<int N, typename T>
inline vecN<N, T> min(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = std::min(x[i], y[i]);
	return r;
}

// min(vector, vector)
template<int N, typename T>
inline T min_component(const vecN<N, T>& x)
{
	T r = x[0];
	for (int i = 1; i < N; ++i) r = std::min(r, x[i]);
	return r;
}

// max(vector, vector)
template<int N, typename T>
inline vecN<N, T> max(const vecN<N, T>& x, const vecN<N, T>& y)
{
	vecN<N, T> r;
	for (int i = 0; i < N; ++i) r[i] = std::max(x[i], y[i]);
	return r;
}

// clamp(vector, vector, vector)
template<int N, typename T>
inline vecN<N, T> clamp(const vecN<N, T>& x, const vecN<N, T>& minVal, const vecN<N, T>& maxVal)
{
	return min(maxVal, max(minVal, x));
}

// clamp(vector, scalar, scalar)
template<int N, typename T>
inline vecN<N, T> clamp(const vecN<N, T>& x, T minVal, T maxVal)
{
	return clamp(x, vecN<N, T>(minVal), vecN<N, T>(maxVal));
}

// sum of components
template<int N, typename T>
inline T sum(const vecN<N, T>& x)
{
	T result(0);
	for (int i = 0; i < N; ++i) result += x[i];
	return result;
}
template<int N, typename T>
inline vecN<N, T> sum_broadcast(const vecN<N, T>& x)
{
	return vecN<N, T>(sum(x));
}

// dot product
template<int N, typename T>
inline T dot(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return sum(x * y);
}
template<int N, typename T>
inline vecN<N, T> dot_broadcast(const vecN<N, T>& x, const vecN<N, T>& y)
{
	return sum_broadcast(x * y);
}

// squared length (norm-2)
template<int N, typename T>
inline T length2(const vecN<N, T>& x)
{
	return dot(x, x);
}

// length (norm-2)
template<int N, typename T>
inline T length(const vecN<N, T>& x)
{
	return sqrt(length2(x));
}

// normalize vector
template<int N, typename T>
inline vecN<N, T> normalize(const vecN<N, T>& x)
{
	return x * rsqrt(length2(x));
}

// cross product (only defined for vectors of length 3)
template<typename T>
inline vecN<3, T> cross(const vecN<3, T>& x, const vecN<3, T>& y)
{
	return vecN<3, T>(x[1] * y[2] - x[2] * y[1],
	                  x[2] * y[0] - x[0] * y[2],
	                  x[0] * y[1] - x[1] * y[0]);
}

// round each component to the nearest integer (returns a vector of integers)
template<int N, typename T>
inline vecN<N, int> round(const vecN<N, T>& x)
{
	vecN<N, int> r;
	// note: std::lrint() is more generic (e.g. also works with double),
	// but Dingux doesn't seem to have std::lrint().
	for (int i = 0; i < N; ++i) r[i] = lrintf(x[i]);
	return r;
}

// truncate each component to the nearest integer that is not bigger in
// absolute value (returns a vector of integers)
template<int N, typename T>
inline vecN<N, int> trunc(const vecN<N, T>& x)
{
	vecN<N, int> r;
	for (int i = 0; i < N; ++i) r[i] = int(x[i]);
	return r;
}

// Textual representation. (Only) used to debug unittest.
template<int N, typename T>
std::ostream& operator<<(std::ostream& os, const vecN<N, T>& x)
{
	os << "[ ";
	for (int i = 0; i < N; ++i) {
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


// --- SSE optimizations ---

#ifdef __SSE__
#include <xmmintrin.h>

// Optionally also use SSE3 and SSE4.1
#ifdef __SSE3__
#include <pmmintrin.h>
#endif
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

namespace gl {

// Specialization: implement a vector of 4 floats using SSE instructions
template<> class vecN<4, float>
{
public:
	vecN()
	{
		e = _mm_setzero_ps();
	}

	explicit vecN(float x)
	{
		e = _mm_set1_ps(x);
	}

	vecN(float x, const vecN<3, float>& yzw)
	{
		e = _mm_setr_ps(x, yzw[0], yzw[1], yzw[2]);
	}

	vecN(const vecN<3, float>& xyz, float w)
	{
		e = _mm_setr_ps(xyz[0], xyz[1], xyz[2], w);
	}

	vecN(const vecN<2, float>& xy, const vecN<2, float>& zw)
	{
		e = _mm_setr_ps(xy[0], xy[1], zw[0], zw[1]);
	}

	vecN(float x, float y, float z, float w)
	{
		e = _mm_setr_ps(x, y, z, w);
	}

	float  operator[](int i) const { return e_[i]; }
	float& operator[](int i)       { return e_[i]; }

	// For structured bindings
	template<size_t I> float  get() const noexcept { return (*this)[I]; }
	template<size_t I> float& get()       noexcept { return (*this)[I]; }

	vecN& operator+=(vecN  x) { *this = *this + x; ; return *this; }
	vecN& operator-=(vecN  x) { *this = *this - x; ; return *this; }
	vecN& operator*=(vecN  x) { *this = *this * x; ; return *this; }
	vecN& operator*=(float x) { *this = *this * x; ; return *this; }

	explicit vecN(__m128 x) : e(x) {}
	__m128 sse() const { return e; }

private:
	// With gcc we don't need this union. With clang we need it to
	// be able to write individual components.
	union {
		__m128 e;
		float e_[4];
	};
};

inline bool operator==(vec4 x, vec4 y)
{
	return _mm_movemask_ps(_mm_cmpeq_ps(x.sse(), y.sse())) == 15;
}

inline vec4 operator+(vec4 x, vec4 y)
{
	return vec4(_mm_add_ps(x.sse(), y.sse()));
}

inline vec4 operator-(vec4 x, vec4 y)
{
	return vec4(_mm_sub_ps(x.sse(), y.sse()));
}

inline vec4 operator*(float x, vec4 y)
{
	return vec4(_mm_mul_ps(_mm_set1_ps(x), y.sse()));
}

inline vec4 operator*(vec4 x, float y)
{
	return vec4(_mm_mul_ps(x.sse(), _mm_set1_ps(y)));
}

inline vec4 operator*(vec4 x, vec4 y)
{
	return vec4(_mm_mul_ps(x.sse(), y.sse()));
}

#ifdef __SSE3__
inline float sum(vec4 x)
{
	__m128 t = _mm_hadd_ps(x.sse(), x.sse());
	return _mm_cvtss_f32(_mm_hadd_ps(t, t));
}
inline vec4 sum_broadcast(vec4 x)
{
	__m128 t = _mm_hadd_ps(x.sse(), x.sse());
	return vec4(_mm_hadd_ps(t, t));
}
#else
inline float sum(vec4 x)
{
	__m128 t0 = x.sse();
	__m128 t1 = _mm_add_ps(t0, _mm_movehl_ps (t0, t0));
	__m128 t2 = _mm_add_ps(t1, _mm_shuffle_ps(t1, t1, _MM_SHUFFLE(1,1,1,1)));
	return _mm_cvtss_f32(t2);
}
inline vec4 sum_broadcast(vec4 x)
{
	__m128 t0 = x.sse();
	__m128 t1 = _mm_add_ps(t0, _mm_shuffle_ps(t0, t0, _MM_SHUFFLE(2,3,0,1)));
	__m128 t2 = _mm_add_ps(t1, _mm_shuffle_ps(t1, t1, _MM_SHUFFLE(1,0,3,2)));
	return vec4(t2);
}
#endif

inline vec4 min(vec4 x, vec4 y)
{
	return vec4(_mm_min_ps(x.sse(), y.sse()));
}

inline vec4 max(vec4 x, vec4 y)
{
	return vec4(_mm_max_ps(x.sse(), y.sse()));
}

#ifdef __SSE4_1__
inline float dot(vec4 x, vec4 y)
{
	return _mm_cvtss_f32(_mm_dp_ps(x.sse(), y.sse(), 0xF1));
}
inline vec4 dot_broadcast(vec4 x, vec4 y)
{
	return vec4(_mm_dp_ps(x.sse(), y.sse(), 0xFF));
}
#endif

inline vec4 normalize(vec4 x)
{
	// Use 1 Newton-Raphson step to improve 1/sqrt(a) approximation:
	//   let s0 be the initial approximation, then
	//       s1 = (3 - s0^2 * a) * (s0 / 2)   is a better approximation
	vec4 l2 = dot_broadcast(x, x);
	__m128 s0 = _mm_rsqrt_ps(l2.sse());
	__m128 ss = _mm_mul_ps(s0, s0);
	__m128 h  = _mm_mul_ps(_mm_set1_ps(-0.5f), s0);
	__m128 m3 = _mm_sub_ps(_mm_mul_ps(l2.sse(), ss), _mm_set1_ps(3.0f));
	__m128 s1 = _mm_mul_ps(h, m3);
	return vec4(_mm_mul_ps(x.sse(), s1));
}

inline vec4 recip(vec4 a)
{
	// Use 1 Newton-Raphson step to improve 1/a approximation:
	//   let x0 be the initial approximation, then
	//       x1 = x0 + x0 - a * x0 * x0   is a better approximation
	__m128 x0 = _mm_rcp_ps(a.sse());
	__m128 m0 = _mm_mul_ps(x0, a.sse());
	__m128 m1 = _mm_mul_ps(x0, m0);
	__m128 a0 = _mm_add_ps(x0, x0);
	__m128 x1 = _mm_sub_ps(a0, m1);
	return vec4(x1);
}

} // namespace gl

#endif // __SSE__

#endif // GL_VEC_HH
