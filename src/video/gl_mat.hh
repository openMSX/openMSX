#ifndef GL_MAT_HH
#define GL_MAT_HH

// This code implements a mathematical matrix, comparable in functionality
// and syntax to the matrix types in GLSL.
//
// The code was tuned for 4x4 matrixes, but when it didn't complicate the
// code, other sizes (even non-square) are supported as well.
//
// The functionality of this code is focused on openGL(ES). So only the
// operation 'matrix x column-vector' is supported and not e.g. 'row-vector x
// matrix'. The internal layout is also compatible with openGL(ES).
//
// In the past we had (some) manual SSE optimizations in this code. Though for
// the functions that matter (matrix-vector and matrix-matrix multiplication),
// the compiler's auto-vectorization has become as good as the manually
// vectorized code.

#include "gl_vec.hh"
#include "xrange.hh"
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
	constexpr matMxN()
	{
		for (auto i : xrange(N)) {
			for (auto j : xrange(M)) {
				c[i][j] = (i == j) ? T(1) : T(0);
			}
		}
	}

	// Construct diagonal matrix.
	constexpr explicit matMxN(const vecN<(M < N ? M : N), T>& d)
	{
		for (auto i : xrange(N)) {
			for (auto j : xrange(M)) {
				c[i][j] = (i == j) ? d[i] : T(0);
			}
		}
	}

	// Construct from larger matrix (higher order elements are dropped).
	template<int M2, int N2> constexpr explicit matMxN(const matMxN<M2, N2, T>& x)
	{
		static_assert(((M2 > M) && (N2 >= N)) || ((M2 >= M) && (N2 > N)),
		              "matrix must have strictly larger dimensions");
		for (auto i : xrange(N)) c[i] = vecN<M, T>(x[i]);
	}

	// Construct matrix from 2 given columns (only valid when N == 2).
	constexpr matMxN(const vecN<M, T>& x, const vecN<M, T>& y)
	{
		static_assert(N == 2, "wrong #constructor arguments");
		c[0] = x; c[1] = y;
	}

	// Construct matrix from 3 given columns (only valid when N == 3).
	constexpr matMxN(const vecN<M, T>& x, const vecN<M, T>& y, const vecN<M, T>& z)
	{
		static_assert(N == 3, "wrong #constructor arguments");
		c[0] = x; c[1] = y; c[2] = z;
	}

	// Construct matrix from 4 given columns (only valid when N == 4).
	constexpr matMxN(const vecN<M, T>& x, const vecN<M, T>& y,
	                 const vecN<M, T>& z, const vecN<M, T>& w)
	{
		static_assert(N == 4, "wrong #constructor arguments");
		c[0] = x; c[1] = y; c[2] = z; c[3] = w;
	}

	// Access the i-the column of the matrix.
	// Vectors are also indexable, so 'A[i][j]' returns the element
	// at the i-th column, j-th row.
	[[nodiscard]] constexpr const vecN<M, T>& operator[](unsigned i) const {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return c[i];
	}
	[[nodiscard]] constexpr vecN<M, T>& operator[](unsigned i) {
		#ifdef DEBUG
		assert(i < N);
		#endif
		return c[i];
	}

	[[nodiscard]] constexpr const T* data() const { return c[0].data(); }
	[[nodiscard]] constexpr       T* data()       { return c[0].data(); }


	// Assignment version of the +,-,* operations defined below.
	constexpr matMxN& operator+=(const matMxN& x) { *this = *this + x; return *this; }
	constexpr matMxN& operator-=(const matMxN& x) { *this = *this - x; return *this; }
	constexpr matMxN& operator*=(T             x) { *this = *this * x; return *this; }
	constexpr matMxN& operator*=(const matMxN<N, N, T>& x) { *this = *this * x; return *this; }

	[[nodiscard]] constexpr bool operator==(const matMxN&) const = default;

	// matrix + matrix
	[[nodiscard]] constexpr friend matMxN operator+(const matMxN& A, const matMxN& B) {
		matMxN<M, N, T> R;
		for (auto i : xrange(N)) R[i] = A[i] + B[i];
		return R;
	}

	// matrix - matrix
	[[nodiscard]] constexpr friend matMxN operator-(const matMxN& A, const matMxN& B) {
		matMxN<M, N, T> R;
		for (auto i : xrange(N)) R[i] = A[i] - B[i];
		return R;
	}

	// scalar * matrix
	[[nodiscard]] constexpr friend matMxN operator*(T x, const matMxN& A) {
		matMxN<M, N, T> R;
		for (auto i : xrange(N)) R[i] = x * A[i];
		return R;
	}

	// matrix * scalar
	[[nodiscard]] constexpr friend matMxN operator*(const matMxN& A, T x) {
		matMxN<M, N, T> R;
		for (auto i : xrange(N)) R[i] = A[i] * x;
		return R;
	}

	// matrix * column-vector
	[[nodiscard]] constexpr friend vecN<M, T> operator*(const matMxN& A, const vecN<N, T>& x) {
		vecN<M, T> r;
		for (auto i : xrange(N)) r += A[i] * x[i];
		return r;
	}

	// matrix * matrix
	template<int O>
	[[nodiscard]] constexpr friend matMxN<M, O, T> operator*(const matMxN& A, const matMxN<N, O, T>& B) {
		matMxN<M, O, T> R;
		for (auto i : xrange(O)) R[i] = A * B[i];
		return R;
	}

	// Textual representation. (Only) used to debug unittest.
	friend std::ostream& operator<<(std::ostream& os, const matMxN& A) {
		for (auto j : xrange(M)) {
			for (auto i : xrange(N)) {
				os << A[i][j] << ' ';
			}
			os << '\n';
		}
		return os;
	}

private:
	std::array<vecN<M, T>, N> c;
};


// Convenience typedefs (same names as used by GLSL).
using mat2 = matMxN<2, 2, float>;
using mat3 = matMxN<3, 3, float>;
using mat4 = matMxN<4, 4, float>;


// -- Matrix functions --

// matrix negation
template<int M, int N, typename T>
[[nodiscard]] constexpr matMxN<M, N, T> operator-(const matMxN<M, N, T>& A)
{
	return matMxN<M, N, T>(vecN<(M < N ? M : N), T>()) - A;
}

// matrix transpose
template<int M, int N, typename T>
[[nodiscard]] constexpr matMxN<N, M, T> transpose(const matMxN<M, N, T>& A)
{
	matMxN<N, M, T> R;
	for (auto i : xrange(N)) {
		for (auto j : xrange(M)) {
			R[j][i] = A[i][j];
		}
	}
	return R;
}

// determinant of a 2x2 matrix
template<typename T>
[[nodiscard]] constexpr T determinant(const matMxN<2, 2, T>& A)
{
	return A[0][0] * A[1][1] - A[0][1] * A[1][0];
}

// determinant of a 3x3 matrix
template<typename T>
[[nodiscard]] constexpr T determinant(const matMxN<3, 3, T>& A)
{
	return A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
	     - A[1][0] * (A[0][1] * A[2][2] - A[0][2] * A[2][1])
	     + A[2][0] * (A[0][1] * A[1][2] - A[0][2] * A[1][1]);
}

// determinant of a 4x4 matrix
template<typename T>
[[nodiscard]] constexpr T determinant(const matMxN<4, 4, T>& A)
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
[[nodiscard]] constexpr matMxN<2, 2, T> inverse(const matMxN<2, 2, T>& A)
{
	T d = T(1) / determinant(A);
	return d * matMxN<2, 2, T>(vecN<2, T>( A[1][1], -A[0][1]),
	                           vecN<2, T>(-A[1][0],  A[0][0]));
}

// inverse of a 3x3 matrix
template<typename T>
[[nodiscard]] constexpr matMxN<3, 3, T> inverse(const matMxN<3, 3, T>& A)
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
[[nodiscard]] constexpr matMxN<4, 4, T> inverse(const matMxN<4, 4, T>& A)
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
[[nodiscard]] constexpr T norm2_2(const matMxN<M, N, T>& A)
{
	vecN<M, T> t;
	for (auto i : xrange(N)) t += A[i] * A[i];
	return sum(t);
}

} // namespace gl

#endif
