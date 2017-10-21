#ifndef GL_MAT_HH
#define GL_MAT_HH

// This code implements a mathematical matrix, comparable in functionality
// and syntax to the matrix types in GLSL. 
// 
// The code was tuned for 4x4 matrixes, but when it didn't complicate the
// code other sizes (even non-square) are supported as well.
//
// The functionality of this code is focused on openGL(ES). So only the
// operation 'matrix x column-vector' is supported and not e.g. 'row-vector x
// matrix'. The internal layout is also compatible with openGL(ES).
//
// This code itself doesn't have many SSE optimizations but because it is built
// on top of an optimized vector type the generated code for 4x4 matrix
// multiplication is as efficient as hand-written SSE assembly routine
// (verified with gcc-4.8 on x86_64). The efficiency for other matrix sizes was
// not a goal for this code (smaller sizes might be ok, bigger sizes most
// likely not).

#include "gl_vec.hh"
#include <cassert>

namespace gl {

// Matrix with M rows and N columns (M by N), elements have type T.
// Internally elements are stored in column-major order. This is the same
// convention as openGL (and Fortran), but different from the typical row-major
// order in C++. This allows to directly pass these matrices to openGL(ES).
template<int M, int N, typename T> class matMxN
{
public:
	// Default copy-constructor and assignment operator.

	// Construct identity matrix.
	matMxN()
	{
		for (int i = 0; i < N; ++i) {
			for (int j = 0; j < M; ++j) {
				c[i][j] = (i == j) ? T(1) : T(0);
			}
		}
	}
	
	// Construct diagonal matrix.
	explicit matMxN(const vecN<(M < N ? M : N), T>& d)
	{
		for (int i = 0; i < N; ++i) {
			for (int j = 0; j < M; ++j) {
				c[i][j] = (i == j) ? d[i] : T(0);
			}
		}
	}

	// Construct from larger matrix (higher order elements are dropped).
	template<int M2, int N2> explicit matMxN(const matMxN<M2, N2, T>& x)
	{
		static_assert(((M2 > M) && (N2 >= N)) || ((M2 >= M) && (N2 > N)),
		              "matrix must have strictly larger dimensions");
		for (int i = 0; i < N; ++i) c[i] = vecN<M, T>(x[i]);
	}

	// Construct matrix from 2 given columns (only valid when N == 2).
	matMxN(const vecN<M, T>& x, const vecN<M, T>& y)
	{
		static_assert(N == 2, "wrong #constructor arguments");
		c[0] = x; c[1] = y;
	}

	// Construct matrix from 3 given columns (only valid when N == 3).
	matMxN(const vecN<M, T>& x, const vecN<M, T>& y, const vecN<M, T>& z)
	{
		static_assert(N == 3, "wrong #constructor arguments");
		c[0] = x; c[1] = y; c[2] = z;
	}

	// Construct matrix from 4 given columns (only valid when N == 4).
	matMxN(const vecN<M, T>& x, const vecN<M, T>& y,
	       const vecN<M, T>& z, const vecN<M, T>& w)
	{
		static_assert(N == 4, "wrong #constructor arguments");
		c[0] = x; c[1] = y; c[2] = z; c[3] = w;
	}

	// Access the i-the column of the matrix.
	// Vectors are also indexable, so 'A[i][j]' returns the element
	// at the i-th column, j-th row.
	const vecN<M, T>& operator[](unsigned i) const {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return c[i];
	}
	vecN<M, T>& operator[](unsigned i) {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return c[i];
	}

	// Assignment version of the +,-,* operations defined below.
	matMxN& operator+=(const matMxN& x) { *this = *this + x; return *this; }
	matMxN& operator-=(const matMxN& x) { *this = *this - x; return *this; }
	matMxN& operator*=(T             x) { *this = *this * x; return *this; }
	matMxN& operator*=(const matMxN<N, N, T>& x) { *this = *this * x; return *this; }

private:
	vecN<M, T> c[N];
};


// Specialization for 4x4 matrix.
// This specialization is not needed for correctness, but it does help gcc-4.8
// to generate better code for the constructors (clang-3.4 doesn't need help).
// Maybe remove this specialization in the future?
template<typename T> class matMxN<4, 4, T>
{
public:
	matMxN()
	{
		T one(1); T zero(0);
		c[0] = vecN<4,T>( one, zero, zero, zero);
		c[1] = vecN<4,T>(zero,  one, zero, zero);
		c[2] = vecN<4,T>(zero, zero,  one, zero);
		c[3] = vecN<4,T>(zero, zero, zero,  one);
	}

	explicit matMxN(vecN<4, T> d)
	{
		T zero(0);
		c[0] = vecN<4,T>(d[0], zero, zero, zero);
		c[1] = vecN<4,T>(zero, d[1], zero, zero);
		c[2] = vecN<4,T>(zero, zero, d[2], zero);
		c[3] = vecN<4,T>(zero, zero, zero, d[3]);
	}

	matMxN(vecN<4, T> x, vecN<4, T> y, vecN<4, T> z, vecN<4, T> w)
	{
		c[0] = x; c[1] = y; c[2] = z; c[3] = w;
	}

	vecN<4, T>  operator[](int i) const { return c[i]; }
	vecN<4, T>& operator[](int i)       { return c[i]; }

	// Assignment version of the +,-,* operations defined below.
	matMxN& operator+=(const matMxN& x) { *this = *this + x; return *this; }
	matMxN& operator-=(const matMxN& x) { *this = *this - x; return *this; }
	matMxN& operator*=(T             x) { *this = *this * x; return *this; }
	matMxN& operator*=(const matMxN& x) { *this = *this * x; return *this; }

private:
	vecN<4, T> c[4];
};


// Convenience typedefs (same names as used by GLSL).
using mat2 = matMxN<2, 2, float>;
using mat3 = matMxN<3, 3, float>;
using mat4 = matMxN<4, 4, float>;


// -- Matrix functions --

// matrix equality / inequality
template<int M, int N, typename T>
inline bool operator==(const matMxN<M, N, T>& A, const matMxN<M, N, T>& B)
{
	for (int i = 0; i < N; ++i) if (A[i] != B[i]) return false;
	return true;
}
template<int M, int N, typename T>
inline bool operator!=(const matMxN<M, N, T>& A, const matMxN<M, N, T>& B)
{
	return !(A == B);
}

// matrix + matrix
template<int M, int N, typename T>
inline matMxN<M, N, T> operator+(const matMxN<M, N, T>& A, const matMxN<M, N, T>& B)
{
	matMxN<M, N, T> R;
	for (int i = 0; i < N; ++i) R[i] = A[i] + B[i];
	return R;
}

// matrix - matrix
template<int M, int N, typename T>
inline matMxN<M, N, T> operator-(const matMxN<M, N, T>& A, const matMxN<M, N, T>& B)
{
	matMxN<M, N, T> R;
	for (int i = 0; i < N; ++i) R[i] = A[i] - B[i];
	return R;
}

// matrix negation
template<int M, int N, typename T>
inline matMxN<M, N, T> operator-(const matMxN<M, N, T>& A)
{
	return matMxN<M, N, T>(vecN<(M < N ? M : N), T>()) - A;
}

// scalar * matrix
template<int M, int N, typename T>
inline matMxN<M, N, T> operator*(T x, const matMxN<M, N, T>& A)
{
	matMxN<M, N, T> R;
	for (int i = 0; i < N; ++i) R[i] = x * A[i];
	return R;
}

// matrix * scalar
template<int M, int N, typename T>
inline matMxN<M, N, T> operator*(const matMxN<M, N, T>& A, T x)
{
	matMxN<M, N, T> R;
	for (int i = 0; i < N; ++i) R[i] = A[i] * x;
	return R;
}

// matrix * column-vector
template<int M, int N, typename T>
inline vecN<M, T> operator*(const matMxN<M, N, T>& A, const vecN<N, T>& x)
{
	vecN<M, T> r;
	for (int i = 0; i < N; ++i) r += A[i] * x[i];
	return r;
}
template<typename T>
inline vecN<4, T> operator*(const matMxN<4, 4, T>& A, vecN<4, T> x)
{
	// Small optimization for 4x4 matrix: reassociate add-chain
	return (A[0] * x[0] + A[1] * x[1]) + (A[2] * x[2] + A[3] * x[3]);
}

// matrix * matrix
template<int M, int N, int O, typename T>
inline matMxN<M, O, T> operator*(const matMxN<M, N, T>& A, const matMxN<N, O, T>& B)
{
	matMxN<M, O, T> R;
	for (int i = 0; i < O; ++i) R[i] = A * B[i];
	return R;
}

// matrix transpose
template<int M, int N, typename T>
inline matMxN<N, M, T> transpose(const matMxN<M, N, T>& A)
{
	matMxN<N, M, T> R;
	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			R[j][i] = A[i][j];
		}
	}
	return R;
}

// determinant of a 2x2 matrix
template<typename T>
inline T determinant(const matMxN<2, 2, T>& A)
{
	return A[0][0] * A[1][1] - A[0][1] * A[1][0];
}

// determinant of a 3x3 matrix
template<typename T>
inline T determinant(const matMxN<3, 3, T>& A)
{
	return A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
	     - A[1][0] * (A[0][1] * A[2][2] - A[0][2] * A[2][1])
	     + A[2][0] * (A[0][1] * A[1][2] - A[0][2] * A[1][1]);
}

// determinant of a 4x4 matrix
template<typename T>
inline T determinant(const matMxN<4, 4, T>& A)
{
	// Implementation based on glm:  http://glm.g-truc.net
	T f0 = A[2][2] * A[3][3] - A[3][2] * A[2][3];
	T f1 = A[2][1] * A[3][3] - A[3][1] * A[2][3];
	T f2 = A[2][1] * A[3][2] - A[3][1] * A[2][2];
	T f3 = A[2][0] * A[3][3] - A[3][0] * A[2][3];
	T f4 = A[2][0] * A[3][2] - A[3][0] * A[2][2];
	T f5 = A[2][0] * A[3][1] - A[3][0] * A[2][1];
	vecN<4, T> c((A[1][1] * f0 - A[1][2] * f1 + A[1][3] * f2),
	             (A[1][2] * f3 - A[1][3] * f4 - A[1][0] * f0),
	             (A[1][0] * f1 - A[1][1] * f3 + A[1][3] * f5),
	             (A[1][1] * f4 - A[1][2] * f5 - A[1][0] * f2));
	return dot(A[0], c);
}

// inverse of a 2x2 matrix
template<typename T>
inline matMxN<2, 2, T> inverse(const matMxN<2, 2, T>& A)
{
	T d = T(1) / determinant(A);
	return d * matMxN<2, 2, T>(vecN<2, T>( A[1][1], -A[0][1]),
	                           vecN<2, T>(-A[1][0],  A[0][0]));
}

// inverse of a 3x3 matrix
template<typename T>
inline matMxN<3, 3, T> inverse(const matMxN<3, 3, T>& A)
{
	T d = T(1) / determinant(A);
	return d * matMxN<3, 3, T>(
		vecN<3, T>(A[1][1] * A[2][2] - A[1][2] * A[2][1],
		           A[0][2] * A[2][1] - A[0][1] * A[2][2],
		           A[0][1] * A[1][2] - A[0][2] * A[1][1]),
		vecN<3, T>(A[1][2] * A[2][0] - A[1][0] * A[2][2],
		           A[0][0] * A[2][2] - A[0][2] * A[2][0],
		           A[0][2] * A[1][0] - A[0][0] * A[1][2]),
		vecN<3, T>(A[1][0] * A[2][1] - A[1][1] * A[2][0],
		           A[0][1] * A[2][0] - A[0][0] * A[2][1],
		           A[0][0] * A[1][1] - A[0][1] * A[1][0]));
}

// inverse of a 4x4 matrix
template<typename T>
inline matMxN<4, 4, T> inverse(const matMxN<4, 4, T>& A)
{
	// Implementation based on glm:  http://glm.g-truc.net

	T c00 = A[2][2] * A[3][3] - A[3][2] * A[2][3];
	T c02 = A[1][2] * A[3][3] - A[3][2] * A[1][3];
	T c03 = A[1][2] * A[2][3] - A[2][2] * A[1][3];

	T c04 = A[2][1] * A[3][3] - A[3][1] * A[2][3];
	T c06 = A[1][1] * A[3][3] - A[3][1] * A[1][3];
	T c07 = A[1][1] * A[2][3] - A[2][1] * A[1][3];

	T c08 = A[2][1] * A[3][2] - A[3][1] * A[2][2];
	T c10 = A[1][1] * A[3][2] - A[3][1] * A[1][2];
	T c11 = A[1][1] * A[2][2] - A[2][1] * A[1][2];

	T c12 = A[2][0] * A[3][3] - A[3][0] * A[2][3];
	T c14 = A[1][0] * A[3][3] - A[3][0] * A[1][3];
	T c15 = A[1][0] * A[2][3] - A[2][0] * A[1][3];

	T c16 = A[2][0] * A[3][2] - A[3][0] * A[2][2];
	T c18 = A[1][0] * A[3][2] - A[3][0] * A[1][2];
	T c19 = A[1][0] * A[2][2] - A[2][0] * A[1][2];

	T c20 = A[2][0] * A[3][1] - A[3][0] * A[2][1];
	T c22 = A[1][0] * A[3][1] - A[3][0] * A[1][1];
	T c23 = A[1][0] * A[2][1] - A[2][0] * A[1][1];

	vecN<4, T> f0(c00, c00, c02, c03);
	vecN<4, T> f1(c04, c04, c06, c07);
	vecN<4, T> f2(c08, c08, c10, c11);
	vecN<4, T> f3(c12, c12, c14, c15);
	vecN<4, T> f4(c16, c16, c18, c19);
	vecN<4, T> f5(c20, c20, c22, c23);

	vecN<4, T> v0(A[1][0], A[0][0], A[0][0], A[0][0]);
	vecN<4, T> v1(A[1][1], A[0][1], A[0][1], A[0][1]);
	vecN<4, T> v2(A[1][2], A[0][2], A[0][2], A[0][2]);
	vecN<4, T> v3(A[1][3], A[0][3], A[0][3], A[0][3]);

	vecN<4, T> i0(v1 * f0 - v2 * f1 + v3 * f2);
	vecN<4, T> i1(v0 * f0 - v2 * f3 + v3 * f4);
	vecN<4, T> i2(v0 * f1 - v1 * f3 + v3 * f5);
	vecN<4, T> i3(v0 * f2 - v1 * f4 + v2 * f5);

	vecN<4, T> sa(+1, -1, +1, -1);
	vecN<4, T> sb(-1, +1, -1, +1);
	matMxN<4, 4, T> inverse(i0 * sa, i1 * sb, i2 * sa, i3 * sb);

	vecN<4, T> row0(inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0]);
	T OneOverDeterminant = static_cast<T>(1) / dot(A[0], row0);

	return inverse * OneOverDeterminant;
}

// norm-2 squared
template<int M, int N, typename T>
inline T norm2_2(const matMxN<M, N, T>& A)
{
	vecN<M, T> t;
	for (int i = 0; i < N; ++i) t += A[i] * A[i];
	return sum(t);
}

} // namespace gl


// --- SSE optimizations ---

#ifdef __SSE__
#include <xmmintrin.h>

namespace gl {

// transpose of 4x4-float matrix
inline mat4 transpose(const mat4& A)
{
	mat4 R;
	__m128 t0 = _mm_shuffle_ps(A[0].sse(), A[1].sse(), _MM_SHUFFLE(1,0,1,0));
	__m128 t1 = _mm_shuffle_ps(A[0].sse(), A[1].sse(), _MM_SHUFFLE(3,2,3,2));
	__m128 t2 = _mm_shuffle_ps(A[2].sse(), A[3].sse(), _MM_SHUFFLE(1,0,1,0));
	__m128 t3 = _mm_shuffle_ps(A[2].sse(), A[3].sse(), _MM_SHUFFLE(3,2,3,2));
	R[0] = vec4(_mm_shuffle_ps(t0, t2, _MM_SHUFFLE(2,0,2,0)));
	R[1] = vec4(_mm_shuffle_ps(t0, t2, _MM_SHUFFLE(3,1,3,1)));
	R[2] = vec4(_mm_shuffle_ps(t1, t3, _MM_SHUFFLE(2,0,2,0)));
	R[3] = vec4(_mm_shuffle_ps(t1, t3, _MM_SHUFFLE(3,1,3,1)));
	return R;
}

// inverse of a 4x4-float matrix
inline mat4 inverse(const mat4& A)
{
	// Implementation based on glm:  http://glm.g-truc.net

	__m128 sa0 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 sb0 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 s00 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 s10 = _mm_shuffle_ps(sa0,        sa0,        _MM_SHUFFLE(2,0,0,0));
	__m128 s20 = _mm_shuffle_ps(sb0,        sb0,        _MM_SHUFFLE(2,0,0,0));
	__m128 s30 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 f0  = _mm_sub_ps(_mm_mul_ps(s00, s10), _mm_mul_ps(s20, s30));

	__m128 sa1 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 sb1 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 s01 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 s11 = _mm_shuffle_ps(sa1,        sa1,        _MM_SHUFFLE(2,0,0,0));
	__m128 s21 = _mm_shuffle_ps(sb1,        sb1,        _MM_SHUFFLE(2,0,0,0));
	__m128 s31 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 f1  = _mm_sub_ps(_mm_mul_ps(s01, s11), _mm_mul_ps(s21, s31));

	__m128 sa2 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 sb2 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 s02 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 s12 = _mm_shuffle_ps(sa2,        sa2,        _MM_SHUFFLE(2,0,0,0));
	__m128 s22 = _mm_shuffle_ps(sb2,        sb2,        _MM_SHUFFLE(2,0,0,0));
	__m128 s32 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 f2  = _mm_sub_ps(_mm_mul_ps(s02, s12), _mm_mul_ps(s22, s32));

	__m128 sa3 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 sb3 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s03 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s13 = _mm_shuffle_ps(sa3,        sa3,        _MM_SHUFFLE(2,0,0,0));
	__m128 s23 = _mm_shuffle_ps(sb3,        sb3,        _MM_SHUFFLE(2,0,0,0));
	__m128 s33 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 f3  = _mm_sub_ps(_mm_mul_ps(s03, s13), _mm_mul_ps(s23, s33));

	__m128 sa4 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 sb4 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s04 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s14 = _mm_shuffle_ps(sa4,        sa4,        _MM_SHUFFLE(2,0,0,0));
	__m128 s24 = _mm_shuffle_ps(sb4,        sb4,        _MM_SHUFFLE(2,0,0,0));
	__m128 s34 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 f4  = _mm_sub_ps(_mm_mul_ps(s04, s14), _mm_mul_ps(s24, s34));

	__m128 sa5 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 sb5 = _mm_shuffle_ps(A[3].sse(), A[2].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s05 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 s15 = _mm_shuffle_ps(sa5,        sa5,        _MM_SHUFFLE(2,0,0,0));
	__m128 s25 = _mm_shuffle_ps(sb5,        sb5,        _MM_SHUFFLE(2,0,0,0));
	__m128 s35 = _mm_shuffle_ps(A[2].sse(), A[1].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 f5  = _mm_sub_ps(_mm_mul_ps(s05, s15), _mm_mul_ps(s25, s35));

	__m128 t0 = _mm_shuffle_ps(A[1].sse(), A[0].sse(), _MM_SHUFFLE(0,0,0,0));
	__m128 t1 = _mm_shuffle_ps(A[1].sse(), A[0].sse(), _MM_SHUFFLE(1,1,1,1));
	__m128 t2 = _mm_shuffle_ps(A[1].sse(), A[0].sse(), _MM_SHUFFLE(2,2,2,2));
	__m128 t3 = _mm_shuffle_ps(A[1].sse(), A[0].sse(), _MM_SHUFFLE(3,3,3,3));
	__m128 v0 = _mm_shuffle_ps(t0, t0, _MM_SHUFFLE(2,2,2,0));
	__m128 v1 = _mm_shuffle_ps(t1, t1, _MM_SHUFFLE(2,2,2,0));
	__m128 v2 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(2,2,2,0));
	__m128 v3 = _mm_shuffle_ps(t3, t3, _MM_SHUFFLE(2,2,2,0));

	__m128 s0 = _mm_sub_ps(_mm_mul_ps(v1, f0), _mm_mul_ps(v2, f1));
	__m128 s1 = _mm_sub_ps(_mm_mul_ps(v0, f0), _mm_mul_ps(v2, f3));
	__m128 s2 = _mm_sub_ps(_mm_mul_ps(v0, f1), _mm_mul_ps(v1, f3));
	__m128 s3 = _mm_sub_ps(_mm_mul_ps(v0, f2), _mm_mul_ps(v1, f4));
	__m128 sa = _mm_set_ps( 1.0f,-1.0f, 1.0f,-1.0f);
	__m128 sb = _mm_set_ps(-1.0f, 1.0f,-1.0f, 1.0f);
	__m128 i0 = _mm_mul_ps(sb, _mm_add_ps(s0, _mm_mul_ps(v3, f2)));
	__m128 i1 = _mm_mul_ps(sa, _mm_add_ps(s1, _mm_mul_ps(v3, f4)));
	__m128 i2 = _mm_mul_ps(sb, _mm_add_ps(s2, _mm_mul_ps(v3, f5)));
	__m128 i3 = _mm_mul_ps(sa, _mm_add_ps(s3, _mm_mul_ps(v2, f5)));

	__m128 ra = _mm_shuffle_ps(i0, i1, _MM_SHUFFLE(0,0,0,0));
	__m128 rb = _mm_shuffle_ps(i2, i3, _MM_SHUFFLE(0,0,0,0));
	vec4 row0 = vec4(_mm_shuffle_ps(ra, rb, _MM_SHUFFLE(2,0,2,0)));

	vec4 rd =  recip(dot_broadcast(A[0], row0));
	return mat4(vec4(i0) * rd, vec4(i1) * rd, vec4(i2) * rd, vec4(i3) * rd);
}

} // namespace gl

#endif // __SSE__

#endif
