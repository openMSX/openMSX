#include "gl_mat.hh"
#include <cassert>
#include <iostream>

using namespace gl;

// Not used in test, but useful to debug.
template<int N, typename T> void print(const vecN<N, T>& x)
{
	for (int i = 0; i < N; ++i) {
		std::cout << x[i] << " ";
	}
	std::cout << std::endl;
}
template<int M, int N, typename T> void print(const matMxN<M, N, T>& A)
{
	for (int j = 0; j < M; ++j) {
		for (int i = 0; i < N; ++i) {
			std::cout << A[i][j] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

// Test approximations.
bool approxEq(const mat4& x, const mat4&y)
{
	return norm2_2(x - y) < 1.0e-3f;
}

int main()
{
	// Extra matrix types used in this test.
	using imat2 = matMxN<2, 2, int>;
	using imat3 = matMxN<3, 3, int>;
	using imat4 = matMxN<4, 4, int>;
	using mat32 matMxN<3, 2, float>;
	using mat23 matMxN<2, 3, float>;

	// It's useful to test both integer and float variants because the
	// former are implemented in plain C++ and (only) the latter have SSE
	// optimizations.
	{
		// default constructor
		mat2 m2;
		assert(m2[0] == vec2(1,0));
		assert(m2[1] == vec2(0,1));

		imat2 i2;
		assert(i2[0] == ivec2(1,0));
		assert(i2[1] == ivec2(0,1));

		mat3 m3;
		assert(m3[0] == vec3(1,0,0));
		assert(m3[1] == vec3(0,1,0));
		assert(m3[2] == vec3(0,0,1));

		imat3 i3;
		assert(i3[0] == ivec3(1,0,0));
		assert(i3[1] == ivec3(0,1,0));
		assert(i3[2] == ivec3(0,0,1));

		mat4 m4;
		assert(m4[0] == vec4(1,0,0,0));
		assert(m4[1] == vec4(0,1,0,0));
		assert(m4[2] == vec4(0,0,1,0));
		assert(m4[3] == vec4(0,0,0,1));

		imat4 i4;
		assert(i4[0] == ivec4(1,0,0,0));
		assert(i4[1] == ivec4(0,1,0,0));
		assert(i4[2] == ivec4(0,0,1,0));
		assert(i4[3] == ivec4(0,0,0,1));

		mat32 m32;
		assert(m32[0] == vec3(1,0,0));
		assert(m32[1] == vec3(0,1,0));

		mat23 m23;
		assert(m23[0] == vec2(1,0));
		assert(m23[1] == vec2(0,1));
		assert(m23[2] == vec2(0,0));
	}
	{
		// diagonal constructor
		mat2 m2(vec2(2,3));
		assert(m2[0] == vec2(2,0));
		assert(m2[1] == vec2(0,3));

		imat2 i2(ivec2(2,3));
		assert(i2[0] == ivec2(2,0));
		assert(i2[1] == ivec2(0,3));

		mat3 m3(vec3(2,3,4));
		assert(m3[0] == vec3(2,0,0));
		assert(m3[1] == vec3(0,3,0));
		assert(m3[2] == vec3(0,0,4));

		imat3 i3(ivec3(2,3,4));
		assert(i3[0] == ivec3(2,0,0));
		assert(i3[1] == ivec3(0,3,0));
		assert(i3[2] == ivec3(0,0,4));

		mat4 m4(vec4(2,3,4,5));
		assert(m4[0] == vec4(2,0,0,0));
		assert(m4[1] == vec4(0,3,0,0));
		assert(m4[2] == vec4(0,0,4,0));
		assert(m4[3] == vec4(0,0,0,5));

		imat4 i4(ivec4(2,3,4,5));
		assert(i4[0] == ivec4(2,0,0,0));
		assert(i4[1] == ivec4(0,3,0,0));
		assert(i4[2] == ivec4(0,0,4,0));
		assert(i4[3] == ivec4(0,0,0,5));

		mat32 m32(vec2(2,3));
		assert(m32[0] == vec3(2,0,0));
		assert(m32[1] == vec3(0,3,0));

		mat23 m23(vec2(2,3));
		assert(m23[0] == vec2(2,0));
		assert(m23[1] == vec2(0,3));
		assert(m23[2] == vec2(0,0));
	}
	{
		// construct from columns
		mat2 m2(vec2(2,3),vec2(4,5));
		assert(m2[0] == vec2(2,3));
		assert(m2[1] == vec2(4,5));

		imat2 i2(ivec2(2,3),ivec2(4,5));
		assert(i2[0] == ivec2(2,3));
		assert(i2[1] == ivec2(4,5));

		mat3 m3(vec3(2,3,4),vec3(5,6,7),vec3(8,9,1));
		assert(m3[0] == vec3(2,3,4));
		assert(m3[1] == vec3(5,6,7));
		assert(m3[2] == vec3(8,9,1));

		imat3 i3(ivec3(2,3,4),ivec3(5,6,7),ivec3(8,9,1));
		assert(i3[0] == ivec3(2,3,4));
		assert(i3[1] == ivec3(5,6,7));
		assert(i3[2] == ivec3(8,9,1));

		mat4 m4(vec4(2,3,4,5),vec4(3,4,5,6),vec4(4,5,6,7),vec4(5,6,7,8));
		assert(m4[0] == vec4(2,3,4,5));
		assert(m4[1] == vec4(3,4,5,6));
		assert(m4[2] == vec4(4,5,6,7));
		assert(m4[3] == vec4(5,6,7,8));

		imat4 i4(ivec4(2,3,4,5),ivec4(3,4,5,6),ivec4(4,5,6,7),ivec4(5,6,7,8));
		assert(i4[0] == ivec4(2,3,4,5));
		assert(i4[1] == ivec4(3,4,5,6));
		assert(i4[2] == ivec4(4,5,6,7));
		assert(i4[3] == ivec4(5,6,7,8));

		mat32 m32(vec3(2,3,4),vec3(5,6,7));
		assert(m32[0] == vec3(2,3,4));
		assert(m32[1] == vec3(5,6,7));

		mat23 m23(vec2(2,3),vec2(4,5),vec2(6,7));
		assert(m23[0] == vec2(2,3));
		assert(m23[1] == vec2(4,5));
		assert(m23[2] == vec2(6,7));
	}
	{
		// modify columns or elements
		mat2 m2(vec2(2,3));
		m2[1] = vec2(8,9); m2[0][1] = 7;
		assert(m2[0] == vec2(2,7));
		assert(m2[1] == vec2(8,9));

		imat2 i2(ivec2(2,3));
		i2[0] = ivec2(8,9); i2[1][1] = 7;
		assert(i2[0] == ivec2(8,9));
		assert(i2[1] == ivec2(0,7));

		mat3 m3(vec3(2,3,4));
		m3[1] = vec3(7,8,9); m3[0][1] = 6;
		assert(m3[0] == vec3(2,6,0));
		assert(m3[1] == vec3(7,8,9));
		assert(m3[2] == vec3(0,0,4));

		imat3 i3(ivec3(2,3,4));
		i3[0] = ivec3(7,8,9); i3[2][0] = 6;
		assert(i3[0] == ivec3(7,8,9));
		assert(i3[1] == ivec3(0,3,0));
		assert(i3[2] == ivec3(6,0,4));

		mat4 m4(vec4(2,3,4,5));
		m4[3] = vec4(6,7,8,9); m4[1][1] = 1;
		assert(m4[0] == vec4(2,0,0,0));
		assert(m4[1] == vec4(0,1,0,0));
		assert(m4[2] == vec4(0,0,4,0));
		assert(m4[3] == vec4(6,7,8,9));

		imat4 i4(ivec4(2,3,4,5));
		i4[1] = ivec4(6,7,8,9); i4[2][3] = 1;
		assert(i4[0] == ivec4(2,0,0,0));
		assert(i4[1] == ivec4(6,7,8,9));
		assert(i4[2] == ivec4(0,0,4,1));
		assert(i4[3] == ivec4(0,0,0,5));

		mat32 m32(vec2(2,3));
		m32[0] = vec3(7,8,9); m32[1][0] = 6;
		assert(m32[0] == vec3(7,8,9));
		assert(m32[1] == vec3(6,3,0));

		mat23 m23(vec2(2,3));
		m23[1] = vec2(8,9); m23[1][0] = 7;
		assert(m23[0] == vec2(2,0));
		assert(m23[1] == vec2(7,9));
		assert(m23[2] == vec2(0,0));
	}
	{
		// (in)equality
		mat3 m3(vec3(1,2,3),vec3(4,5,6),vec3(7,8,9));
		mat3 n3(vec3(1,0,3),vec3(4,5,6),vec3(7,8,9));
		mat3 o3(vec3(1,2,3),vec3(4,5,0),vec3(7,8,9));
		mat3 p3(vec3(1,2,3),vec3(4,5,6),vec3(7,0,9));
		assert(m3 == m3);
		assert(m3 != n3);
		assert(m3 != o3);
		assert(m3 != p3);

		imat3 i3(ivec3(1,2,3),ivec3(4,5,6),ivec3(7,8,9));
		imat3 j3(ivec3(1,2,3),ivec3(4,5,6),ivec3(7,0,9));
		imat3 k3(ivec3(1,2,3),ivec3(4,5,0),ivec3(7,8,9));
		imat3 l3(ivec3(0,2,3),ivec3(4,5,6),ivec3(7,8,9));
		assert(i3 == i3);
		assert(i3 != j3);
		assert(i3 != k3);
		assert(i3 != l3);

		mat4 m4(vec4(1,2,3,4),vec4(3,4,5,6),vec4(5,6,7,8),vec4(7,8,9,0));
		mat4 n4(vec4(1,2,0,4),vec4(3,4,5,6),vec4(5,6,7,8),vec4(7,8,9,0));
		mat4 o4(vec4(1,2,3,4),vec4(0,4,5,6),vec4(5,6,7,8),vec4(7,8,9,0));
		mat4 p4(vec4(1,2,3,4),vec4(3,4,5,6),vec4(5,0,7,8),vec4(7,8,9,0));
		mat4 q4(vec4(1,2,3,4),vec4(3,4,5,6),vec4(5,6,7,8),vec4(7,8,9,1));
		assert(m4 == m4);
		assert(m4 != n4);
		assert(m4 != o4);
		assert(m4 != p4);
		assert(m4 != q4);

		imat4 i4(ivec4(1,2,3,4),ivec4(3,4,5,6),ivec4(5,6,7,8),ivec4(7,8,9,0));
		imat4 j4(ivec4(1,0,3,4),ivec4(3,4,5,6),ivec4(5,6,7,8),ivec4(7,8,9,0));
		imat4 k4(ivec4(1,2,3,4),ivec4(3,4,0,6),ivec4(5,6,7,8),ivec4(7,8,9,0));
		imat4 l4(ivec4(1,2,3,4),ivec4(3,4,5,6),ivec4(5,6,7,0),ivec4(7,8,9,0));
		imat4 h4(ivec4(1,2,3,4),ivec4(3,4,5,6),ivec4(5,6,7,8),ivec4(0,8,9,0));
		assert(i4 == i4);
		assert(i4 != j4);
		assert(i4 != k4);
		assert(i4 != l4);
		assert(i4 != h4);

		mat32 m32(vec3(2,3,4),vec3(5,6,7));
		mat32 n32(vec3(2,3,0),vec3(5,6,7));
		mat32 o32(vec3(2,3,4),vec3(0,6,7));
		assert(m32 == m32);
		assert(m32 != n32);
		assert(m32 != o32);

		mat23 m23(vec2(2,3),vec2(4,5),vec2(6,7));
		mat23 n23(vec2(0,3),vec2(4,5),vec2(6,7));
		mat23 o23(vec2(2,3),vec2(4,0),vec2(6,7));
		mat23 p23(vec2(2,3),vec2(4,5),vec2(0,7));
		assert(m23 == m23);
		assert(m23 != n23);
		assert(m23 != o23);
		assert(m23 != p23);
	}
	{
		// copy constructor, assignment
		mat2 m2(vec2(2,3));
		mat2 n2(m2);
		assert(n2 == m2);
		m2[1][0] = 9; n2 = m2;
		assert(n2 == m2);

		imat2 i2(ivec2(2,3));
		imat2 j2(i2);
		assert(j2 == i2);
		i2[0][1] = 9; j2 = i2;
		assert(j2 == i2);

		mat3 m3(vec3(3,4,5));
		mat3 n3(m3);
		assert(n3 == m3);
		m3[2][1] = 8; n3 = m3;
		assert(n3 == m3);

		imat3 i3(ivec3(3,4,5));
		imat3 j3(i3);
		assert(j3 == i3);
		i3[1][2] = 8; j3 = i3;
		assert(j3 == i3);

		mat4 m4(vec4(4,5,6,7));
		mat4 n4(m4);
		assert(n4 == m4);
		m3[3][1] = 2; n4 = m4;
		assert(n4 == m4);

		imat4 i4(ivec4(4,5,6,7));
		imat4 j4(i4);
		assert(i4 == j4);
		i3[0][1] = 1; j4 = i4;
		assert(i4 == j4);
	}
	{
		// construct from larger matrix
		mat4 m4(vec4(1,2,3,4),vec4(4,5,6,7),vec4(7,8,9,1),vec4(1,0,0,0));
		mat3 m3(vec3(1,2,3),vec3(4,5,6),vec3(7,8,9));
		mat2 m2(vec2(1,2),vec2(4,5));
		assert(mat3(m4) == m3);
		assert(mat2(m4) == m2);
		assert(mat2(m3) == m2);

		imat4 i4(ivec4(1,2,3,4),ivec4(4,5,6,7),ivec4(7,8,9,1),ivec4(1,0,0,0));
		imat3 i3(ivec3(1,2,3),ivec3(4,5,6),ivec3(7,8,9));
		imat2 i2(ivec2(1,2),ivec2(4,5));
		assert(imat3(i4) == i3);
		assert(imat2(i4) == i2);
		assert(imat2(i3) == i2);

		assert(mat32(m4) == mat32(vec3(1,2,3),vec3(4,5,6)));
		assert(mat32(m3) == mat32(vec3(1,2,3),vec3(4,5,6)));
		assert(mat23(m4) == mat23(vec2(1,2),vec2(4,5),vec2(7,8)));
		assert(mat23(m3) == mat23(vec2(1,2),vec2(4,5),vec2(7,8)));
	}
	{
		// matrix addition, subtraction, negation
		mat3 m3(vec3(1,2,3),vec3(4,5,6),vec3(7,8,9));
		mat3 n3(vec3(2,4,6),vec3(1,3,5),vec3(7,8,9));
		mat3 o3 = m3 + n3;
		mat3 p3 = m3 - n3;
		assert(o3 == mat3(vec3(3,6,9),vec3(5,8,11),vec3(14,16,18)));
		assert(p3 == mat3(vec3(-1,-2,-3),vec3(3,2,1),vec3(0,0,0)));
		p3 += n3;
		o3 -= n3;
		assert(o3 == m3);
		assert(p3 == m3);
		assert(-m3 == mat3(vec3(-1,-2,-3),vec3(-4,-5,-6),vec3(-7,-8,-9)));

		imat3 i3(ivec3(6,3,0),ivec3(2,4,6),ivec3(3,4,5));
		imat3 j3(ivec3(1,2,3),ivec3(4,0,4),ivec3(0,1,1));
		imat3 k3 = i3 + j3;
		imat3 l3 = i3 - j3;
		assert(k3 == imat3(ivec3(7,5,3),ivec3(6,4,10),ivec3(3,5,6)));
		assert(l3 == imat3(ivec3(5,1,-3),ivec3(-2,4,2),ivec3(3,3,4)));
		l3 += j3;
		k3 -= j3;
		assert(k3 == i3);
		assert(l3 == i3);
		assert(-i3 == imat3(ivec3(-6,-3,-0),ivec3(-2,-4,-6),ivec3(-3,-4,-5)));

		mat4 m4(vec4(1,2,3,4),vec4(4,5,6,7),vec4(7,8,9,1),vec4(1,0,0,0));
		mat4 n4(vec4(2,4,6,8),vec4(1,3,5,7),vec4(7,8,9,2),vec4(0,0,0,1));
		mat4 o4 = m4 + n4;
		mat4 p4 = m4 - n4;
		assert(o4 == mat4(vec4(3,6,9,12),vec4(5,8,11,14),vec4(14,16,18,3),vec4(1,0,0,1)));
		assert(p4 == mat4(vec4(-1,-2,-3,-4),vec4(3,2,1,0),vec4(0,0,0,-1),vec4(1,0,0,-1)));
		p4 += n4;
		o4 -= n4;
		assert(o4 == m4);
		assert(p4 == m4);
		assert(-m4 == mat4(vec4(-1,-2,-3,-4),vec4(-4,-5,-6,-7),vec4(-7,-8,-9,-1),vec4(-1,0,0,0)));

		imat4 i4(ivec4(0,1,2,3),ivec4(4,3,2,1),ivec4(1,1,1,1),ivec4(5,5,4,4));
		imat4 j4(ivec4(2,3,4,5),ivec4(1,1,2,2),ivec4(0,0,1,2),ivec4(1,2,2,0));
		imat4 k4 = i4 + j4;
		imat4 l4 = i4 - j4;
		assert(k4 == imat4(ivec4(2,4,6,8),ivec4(5,4,4,3),ivec4(1,1,2,3),ivec4(6,7,6,4)));
		assert(l4 == imat4(ivec4(-2,-2,-2,-2),ivec4(3,2,0,-1),ivec4(1,1,0,-1),ivec4(4,3,2,4)));
		l4 += j4;
		k4 -= j4;
		assert(k4 == i4);
		assert(l4 == i4);
		assert(-i4 == imat4(ivec4(0,-1,-2,-3),ivec4(-4,-3,-2,-1),ivec4(-1,-1,-1,-1),ivec4(-5,-5,-4,-4)));

		mat32 m32(vec3(3,4,5),vec3(4,2,0));
		mat32 n32(vec3(1,1,2),vec3(4,4,3));
		mat32 o32 = m32 + n32;
		mat32 p32 = m32 - n32;
		assert(o32 == mat32(vec3(4,5,7),vec3(8,6,3)));
		assert(p32 == mat32(vec3(2,3,3),vec3(0,-2,-3)));
		p32 += n32;
		o32 -= n32;
		assert(o32 == m32);
		assert(p32 == m32);
		assert(-m32 == mat32(vec3(-3,-4,-5),vec3(-4,-2,0)));
	}
	{
		// matrix * scalar
		mat3 m3(vec3(1,2,3),vec3(4, 5, 6),vec3( 7, 8, 9));
		mat3 n3(vec3(2,4,6),vec3(8,10,12),vec3(14,16,18));
		assert((2.0f * m3) == n3);
		assert((m3 * 2.0f) == n3);
		m3 *= 2.0f;
		assert(m3 == n3);

		imat3 i3(ivec3(2,1,0),ivec3(-1, 5,3),ivec3( 7,0,-3));
		imat3 j3(ivec3(4,2,0),ivec3(-2,10,6),ivec3(14,0,-6));
		assert((2 * i3) == j3);
		assert((i3 * 2) == j3);
		i3 *= 2;
		assert(i3 == j3);

		mat4 m4(vec4(1,2,3,4),vec4(4, 5, 6, 7),vec4( 7, 8, 9,10),vec4(0,0,-1,-2));
		mat4 n4(vec4(2,4,6,8),vec4(8,10,12,14),vec4(14,16,18,20),vec4(0,0,-2,-4));
		assert((2.0f * m4) == n4);
		assert((m4 * 2.0f) == n4);
		m4 *= 2.0f;
		assert(m4 == n4);

		imat4 i4(ivec4(0,1,2,3),ivec4(1,0,0,2),ivec4(3,2,1,0),ivec4(0,0,-1,-2));
		imat4 j4(ivec4(0,3,6,9),ivec4(3,0,0,6),ivec4(9,6,3,0),ivec4(0,0,-3,-6));
		assert((3 * i4) == j4);
		assert((i4 * 3) == j4);
		i4 *= 3;
		assert(i4 == j4);

		mat32 m32(vec3( 3, 4,  5),vec3( 4, 2,0));
		mat32 n32(vec3(-6,-8,-10),vec3(-8,-4,0));
		assert((-2.0f * m32) == n32);
		assert((m32 * -2.0f) == n32);
		m32 *= -2.0f;
		assert(m32 == n32);
	}
	{
		// matrix * column-vector
		mat3 m3(vec3(1,2,3),vec3(4,5,6),vec3(7,8,9));
		assert((m3 * vec3(1,0,0)) == vec3( 1, 2, 3));
		assert((m3 * vec3(0,2,0)) == vec3( 8,10,12));
		assert((m3 * vec3(0,0,3)) == vec3(21,24,27));
		assert((m3 * vec3(1,2,3)) == vec3(30,36,42));

		imat3 i3(ivec3(0,-2,1),ivec3(1,-1,0),ivec3(-1,-2,1));
		assert((i3 * ivec3(-1,0,0)) == ivec3( 0, 2,-1));
		assert((i3 * ivec3( 0,0,0)) == ivec3( 0, 0, 0));
		assert((i3 * ivec3( 0,0,3)) == ivec3(-3,-6, 3));
		assert((i3 * ivec3(-1,0,3)) == ivec3(-3,-4, 2));

		mat4 m4(vec4(1,2,3,4),vec4(4,5,6,7),vec4(7,8,9,1),vec4(0,1,0,-1));
		assert((m4 * vec4(1,0,0,0)) == vec4( 1, 2, 3, 4));
		assert((m4 * vec4(0,2,0,0)) == vec4( 8,10,12,14));
		assert((m4 * vec4(0,0,3,0)) == vec4(21,24,27, 3));
		assert((m4 * vec4(0,0,0,4)) == vec4( 0, 4, 0,-4));
		assert((m4 * vec4(1,2,3,4)) == vec4(30,40,42,17));

		imat4 i4(ivec4(-1,2,0,-3),ivec4(0,1,2,-1),ivec4(0,1,1,0),ivec4(-1,1,0,-1));
		assert((i4 * ivec4(-1,0, 0,0)) == ivec4( 1,-2, 0, 3));
		assert((i4 * ivec4( 0,2, 0,0)) == ivec4( 0, 2, 4,-2));
		assert((i4 * ivec4( 0,0,-2,0)) == ivec4( 0,-2,-2, 0));
		assert((i4 * ivec4( 0,0, 0,1)) == ivec4(-1, 1, 0,-1));
		assert((i4 * ivec4(-1,2,-2,1)) == ivec4( 0,-1, 2, 0));

		mat32 m32(vec3(3,4,5),vec3(4,2,0));
		assert((m32 * vec2(2,3)) == vec3(18,14,10));

		mat23 m23(vec2(3,4),vec2(1,5),vec2(4,2));
		assert((m23 * vec3(1,2,3)) == vec2(17,20));
	}
	{
		// matrix * matrix
		mat3 m3(vec3(1,-1,1),vec3(2,0,-1),vec3(0,1,1));
		mat3 n3(vec3(1,1,-1),vec3(0,-2,1),vec3(1,0,1));
		mat3 o3(vec3(0,-1,2),vec3(1,2,-2),vec3(0,2,1));
		assert((m3 * n3) == mat3(vec3(3,-2,-1),vec3(-4,1,3),vec3(1,0,2)));
		assert((n3 * o3) == mat3(vec3(2,2,1),vec3(-1,-3,-1),vec3(1,-4,3)));
		assert(((m3 * n3) * o3) == (m3 * (n3 * o3)));

		imat3 i3(ivec3(1,1,1),ivec3(-2,0,-1),ivec3(0,-1,1));
		imat3 j3(ivec3(-1,0,-1),ivec3(1,-2,1),ivec3(1,-1,1));
		assert((i3 * j3) == imat3(ivec3(-1,0,-2),ivec3(5,0,4),ivec3(3,0,3)));

		mat4 m4(vec4(1,-1,1,0),vec4(2,0,-1,1),vec4(0,1,1,-1),vec4(2,0,1,-1));
		mat4 n4(vec4(1,1,-1,1),vec4(0,-2,1,0),vec4(1,0,1,-1),vec4(0,1,2,-2));
		assert((m4 * n4) == mat4(vec4(5,-2,0,1),vec4(-4,1,3,-3),vec4(-1,0,1,0),vec4(-2,2,-1,1)));
		assert((n4 * m4) == mat4(vec4(2,3,-1,0),vec4(1,3,-1,1),vec4(1,-3,0,1),vec4(3,1,-3,3)));

		imat4 i4(ivec4(1,-1,1,2),ivec4(2,-1,-1,1),ivec4(2,1,1,-1),ivec4(2,-1,1,-1));
		imat4 j4(ivec4(1,1,-1,1),ivec4(2,-2,1,-1),ivec4(1,-2,1,-1),ivec4(-1,1,2,-2));
		assert((i4 * j4) == imat4(ivec4(3,-4,0,3),ivec4(-2,2,4,2),ivec4(-3,3,3,0),ivec4(1,4,-2,-1)));

		mat32 m32(vec3(1,2,-1),vec3(-1,-1,2));
		mat23 m23(vec2(1,-2),vec2(2,-1),vec2(1,-1));
		assert((m32 * m23) == mat3(vec3(3,4,-5),vec3(3,5,-4),vec3(2,3,-3)));
		assert((m23 * m32) == mat2(vec2(4,-3),vec2(-1,1)));
		assert((m3  * m32) == mat32(vec3(5,-2,-2),vec3(-3,3,2)));
		assert((m23 * m3 ) == mat23(vec2(0,-2),vec2(1,-3),vec2(3,-2)));
	}
	{
		// transpose
		mat3 m3(vec3(1,-1,1),vec3(2,0,-1),vec3(0,1,1));
		assert(transpose(m3) == mat3(vec3(1,2,0),vec3(-1,0,1),vec3(1,-1,1)));

		imat3 i3(ivec3(1,1,1),ivec3(-2,0,-1),ivec3(0,-1,1));
		assert(transpose(i3) == imat3(ivec3(1,-2,0),ivec3(1,0,-1),ivec3(1,-1,1)));

		mat4 m4(vec4(1,-1,1,0),vec4(2,0,-1,1),vec4(0,1,1,-1),vec4(2,0,1,-1));
		assert(transpose(m4) == mat4(vec4(1,2,0,2),vec4(-1,0,1,0),vec4(1,-1,1,1),vec4(0,1,-1,-1)));

		imat4 i4(ivec4(1,-1,1,2),ivec4(2,-1,-1,1),ivec4(2,1,1,-1),ivec4(2,-1,1,-1));
		assert(transpose(i4) == imat4(ivec4(1,2,2,2),ivec4(-1,-1,1,-1),ivec4(1,-1,1,1),ivec4(2,1,-1,-1)));

		mat32 m32(vec3(1,2,3),vec3(4,5,6));
		mat23 m23(vec2(1,4),vec2(2,5),vec2(3,6));
		assert(transpose(m32) == m23);
		assert(transpose(m23) == m32);
	}
	{
		// determinant, inverse
		mat2 m2(vec2(1,-1),vec2(0,1));
		assert(determinant(m2) == 1.0f);
		mat2 i2 = inverse(m2);
		assert(i2 == mat2(vec2(1,1),vec2(0,1)));
		assert(m2*i2 == mat2());
		assert(i2*m2 == mat2());

		mat3 m3(vec3(1,0,-1),vec3(0,1,0),vec3(1,-1,1));
		assert(determinant(m3) == 2.0f);
		mat3 i3 = inverse(m3);
		assert(i3 == mat3(vec3(0.5f,0.5f,0.5f),vec3(0,1,0),vec3(-0.5f,0.5f,0.5f)));
		assert(m3*i3 == mat3());
		assert(i3*m3 == mat3());

		mat4 m4(vec4(0,1,1,0),vec4(-1,0,1,0),vec4(1,1,0,-1),vec4(0,1,0,0));
		assert(determinant(m4) == -1.0f);
		mat4 i4 = inverse(m4);
		assert(approxEq(i4, mat4(vec4(1,-1,0,-1),vec4(0,0,0,1),vec4(1,0,0,-1),vec4(1,-1,-1,0))));
		assert(approxEq(m4*i4, mat4()));
		assert(approxEq(i4*m4, mat4()));
	}
	{
		// norm-2 squared
		mat3 m3(vec3(1,-1,1),vec3(2,0,-1),vec3(0,1,1));
		assert(norm2_2(m3) == 10);

		imat3 i3(ivec3(1,2,1),ivec3(-2,3,-1),ivec3(2,1,4));
		assert(norm2_2(i3) == 41);

		mat4 m4(vec4(1,-1,1,2),vec4(2,0,-1,1),vec4(0,1,1,0),vec4(0,1,1,3));
		assert(norm2_2(m4) == 26);

		imat4 i4(ivec4(1,2,1,-3),ivec4(-2,3,-1,0),ivec4(2,1,4,-2),ivec4(0,2,2,2));
		assert(norm2_2(i4) == 66);

		mat32 m32(vec3(1,2,-1),vec3(-1,-1,2));
		mat23 m23(vec2(1,-1),vec2(2,-1),vec2(-1,2));
		assert(norm2_2(m32) == norm2_2(m23));
	}
}


// The following functions are not part of the actual test. They get compiled,
// but never executed. I used them to (manually) inspect the quality of the
// generated code. Only for mat4, because the code was only optimized for that
// type.

void test_constr(mat4& A)
{
	A = mat4();
}
void test_constr(const vec4& x, mat4& A)
{
	A = mat4(x);
}
void test_constr(const vec4& x, const vec4& y, const vec4& z, const vec4& w, mat4& A)
{
	A = mat4(x, y, z, w);
}
void test_constr(const mat4& A, mat3& B)
{
	B = mat3(A);
}

void test_change0(const vec4& x, mat4& A)
{
	A[0] = x;
}
void test_change2(const vec4& x, mat4& A)
{
	A[2] = x;
}
void test_extr0(const mat4& A, vec4& x)
{
	x = A[0];
}
void test_extr2(const mat4& A, vec4& x)
{
	x = A[2];
}

bool test_equal(const mat4& A, const mat4& B)
{
	return A == B;
}
bool test_not_equal(const mat4& A, const mat4& B)
{
	return A != B;
}

void test_add(const mat4& A, const mat4& B, mat4& C)
{
	C = A + B;
}
void test_add(const mat4& A, mat4& B)
{
	B += A;
}
void test_sub(const mat4& A, const mat4& B, mat4& C)
{
	C = A - B;
}
void test_negate(const mat4& A, mat4& B)
{
	B = -A;
}

void test_mul(float x, const mat4& A, mat4& B)
{
	B = x * A;
}
void test_mul(const mat4& A, const vec4& x, vec4& y)
{
	y = A * x;
}
void test_mul(const mat4& A, const mat4& B, mat4& C)
{
	C = A * B;
}

void test_transpose(const mat4& A, mat4& B)
{
	B = transpose(A);
}

void test_determinant(const mat4& A, float& x)
{
	x = determinant(A);
}

void test_inverse(const mat4& A, mat4& B)
{
	B = inverse(A);
}

void test_norm(const mat4& A, float& x)
{
	x = norm2_2(A);
}
