#include "catch.hpp"
#include "gl_mat.hh"

using namespace gl;

// Test approximations.
static constexpr bool approxEq(const mat4& x, const mat4&y)
{
	return norm2_2(x - y) < 1.0e-3f;
}

// Extra matrix types used in this test.
using imat2 = matMxN<2, 2, int>;
using imat3 = matMxN<3, 3, int>;
using imat4 = matMxN<4, 4, int>;
using mat32 = matMxN<3, 2, float>;
using mat23 = matMxN<2, 3, float>;

// It's useful to test both integer and float variants because the
// former are implemented in plain C++ and (only) the latter have SSE
// optimizations.

TEST_CASE("gl_mat: constructors")
{
	SECTION("default constructor") {
		mat2 m2;
		CHECK(m2[0] == vec2(1, 0));
		CHECK(m2[1] == vec2(0, 1));

		imat2 i2;
		CHECK(i2[0] == ivec2(1, 0));
		CHECK(i2[1] == ivec2(0, 1));

		mat3 m3;
		CHECK(m3[0] == vec3(1, 0, 0));
		CHECK(m3[1] == vec3(0, 1, 0));
		CHECK(m3[2] == vec3(0, 0, 1));

		imat3 i3;
		CHECK(i3[0] == ivec3(1, 0, 0));
		CHECK(i3[1] == ivec3(0, 1, 0));
		CHECK(i3[2] == ivec3(0, 0, 1));

		mat4 m4;
		CHECK(m4[0] == vec4(1, 0, 0, 0));
		CHECK(m4[1] == vec4(0, 1, 0, 0));
		CHECK(m4[2] == vec4(0, 0, 1, 0));
		CHECK(m4[3] == vec4(0, 0, 0, 1));

		imat4 i4;
		CHECK(i4[0] == ivec4(1, 0, 0, 0));
		CHECK(i4[1] == ivec4(0, 1, 0, 0));
		CHECK(i4[2] == ivec4(0, 0, 1, 0));
		CHECK(i4[3] == ivec4(0, 0, 0, 1));

		mat32 m32;
		CHECK(m32[0] == vec3(1, 0, 0));
		CHECK(m32[1] == vec3(0, 1, 0));

		mat23 m23;
		CHECK(m23[0] == vec2(1, 0));
		CHECK(m23[1] == vec2(0, 1));
		CHECK(m23[2] == vec2(0, 0));
	}
	SECTION("diagonal constructor") {
		mat2 m2(vec2(2, 3));
		CHECK(m2[0] == vec2(2, 0));
		CHECK(m2[1] == vec2(0, 3));

		imat2 i2(ivec2(2, 3));
		CHECK(i2[0] == ivec2(2, 0));
		CHECK(i2[1] == ivec2(0, 3));

		mat3 m3(vec3(2, 3, 4));
		CHECK(m3[0] == vec3(2, 0, 0));
		CHECK(m3[1] == vec3(0, 3, 0));
		CHECK(m3[2] == vec3(0, 0, 4));

		imat3 i3(ivec3(2, 3, 4));
		CHECK(i3[0] == ivec3(2, 0, 0));
		CHECK(i3[1] == ivec3(0, 3, 0));
		CHECK(i3[2] == ivec3(0, 0, 4));

		mat4 m4(vec4(2, 3, 4, 5));
		CHECK(m4[0] == vec4(2, 0, 0, 0));
		CHECK(m4[1] == vec4(0, 3, 0, 0));
		CHECK(m4[2] == vec4(0, 0, 4, 0));
		CHECK(m4[3] == vec4(0, 0, 0, 5));

		imat4 i4(ivec4(2, 3, 4, 5));
		CHECK(i4[0] == ivec4(2, 0, 0, 0));
		CHECK(i4[1] == ivec4(0, 3, 0, 0));
		CHECK(i4[2] == ivec4(0, 0, 4, 0));
		CHECK(i4[3] == ivec4(0, 0, 0, 5));

		mat32 m32(vec2(2, 3));
		CHECK(m32[0] == vec3(2, 0, 0));
		CHECK(m32[1] == vec3(0, 3, 0));

		mat23 m23(vec2(2, 3));
		CHECK(m23[0] == vec2(2, 0));
		CHECK(m23[1] == vec2(0, 3));
		CHECK(m23[2] == vec2(0, 0));
	}
	SECTION("construct from columns") {
		mat2 m2(vec2(2, 3), vec2(4, 5));
		CHECK(m2[0] == vec2(2, 3));
		CHECK(m2[1] == vec2(4, 5));

		imat2 i2(ivec2(2, 3), ivec2(4, 5));
		CHECK(i2[0] == ivec2(2, 3));
		CHECK(i2[1] == ivec2(4, 5));

		mat3 m3(vec3(2, 3, 4), vec3(5, 6, 7), vec3(8, 9, 1));
		CHECK(m3[0] == vec3(2, 3, 4));
		CHECK(m3[1] == vec3(5, 6, 7));
		CHECK(m3[2] == vec3(8, 9, 1));

		imat3 i3(ivec3(2, 3, 4), ivec3(5, 6, 7), ivec3(8, 9, 1));
		CHECK(i3[0] == ivec3(2, 3, 4));
		CHECK(i3[1] == ivec3(5, 6, 7));
		CHECK(i3[2] == ivec3(8, 9, 1));

		mat4 m4(vec4(2, 3, 4, 5), vec4(3, 4, 5, 6), vec4(4, 5, 6, 7), vec4(5, 6, 7, 8));
		CHECK(m4[0] == vec4(2, 3, 4, 5));
		CHECK(m4[1] == vec4(3, 4, 5, 6));
		CHECK(m4[2] == vec4(4, 5, 6, 7));
		CHECK(m4[3] == vec4(5, 6, 7, 8));

		imat4 i4(ivec4(2, 3, 4, 5), ivec4(3, 4, 5, 6), ivec4(4, 5, 6, 7), ivec4(5, 6, 7, 8));
		CHECK(i4[0] == ivec4(2, 3, 4, 5));
		CHECK(i4[1] == ivec4(3, 4, 5, 6));
		CHECK(i4[2] == ivec4(4, 5, 6, 7));
		CHECK(i4[3] == ivec4(5, 6, 7, 8));

		mat32 m32(vec3(2, 3, 4), vec3(5, 6, 7));
		CHECK(m32[0] == vec3(2, 3, 4));
		CHECK(m32[1] == vec3(5, 6, 7));

		mat23 m23(vec2(2, 3), vec2(4, 5), vec2(6, 7));
		CHECK(m23[0] == vec2(2, 3));
		CHECK(m23[1] == vec2(4, 5));
		CHECK(m23[2] == vec2(6, 7));
	}
}

TEST_CASE("gl_mat: modify columns or elements")
{
	mat2 m2(vec2(2, 3));
	m2[1] = vec2(8, 9); m2[0][1] = 7;
	CHECK(m2[0] == vec2(2, 7));
	CHECK(m2[1] == vec2(8, 9));

	imat2 i2(ivec2(2, 3));
	i2[0] = ivec2(8, 9); i2[1][1] = 7;
	CHECK(i2[0] == ivec2(8, 9));
	CHECK(i2[1] == ivec2(0, 7));

	mat3 m3(vec3(2, 3, 4));
	m3[1] = vec3(7, 8, 9); m3[0][1] = 6;
	CHECK(m3[0] == vec3(2, 6, 0));
	CHECK(m3[1] == vec3(7, 8, 9));
	CHECK(m3[2] == vec3(0, 0, 4));

	imat3 i3(ivec3(2, 3, 4));
	i3[0] = ivec3(7, 8, 9); i3[2][0] = 6;
	CHECK(i3[0] == ivec3(7, 8, 9));
	CHECK(i3[1] == ivec3(0, 3, 0));
	CHECK(i3[2] == ivec3(6, 0, 4));

	mat4 m4(vec4(2, 3, 4, 5));
	m4[3] = vec4(6, 7, 8, 9); m4[1][1] = 1;
	CHECK(m4[0] == vec4(2, 0, 0, 0));
	CHECK(m4[1] == vec4(0, 1, 0, 0));
	CHECK(m4[2] == vec4(0, 0, 4, 0));
	CHECK(m4[3] == vec4(6, 7, 8, 9));

	imat4 i4(ivec4(2, 3, 4, 5));
	i4[1] = ivec4(6, 7, 8, 9); i4[2][3] = 1;
	CHECK(i4[0] == ivec4(2, 0, 0, 0));
	CHECK(i4[1] == ivec4(6, 7, 8, 9));
	CHECK(i4[2] == ivec4(0, 0, 4, 1));
	CHECK(i4[3] == ivec4(0, 0, 0, 5));

	mat32 m32(vec2(2, 3));
	m32[0] = vec3(7, 8, 9); m32[1][0] = 6;
	CHECK(m32[0] == vec3(7, 8, 9));
	CHECK(m32[1] == vec3(6, 3, 0));

	mat23 m23(vec2(2, 3));
	m23[1] = vec2(8, 9); m23[1][0] = 7;
	CHECK(m23[0] == vec2(2, 0));
	CHECK(m23[1] == vec2(7, 9));
	CHECK(m23[2] == vec2(0, 0));
}

TEST_CASE("gl_mat: (in)equality")
{
	mat3 m3(vec3(1, 2, 3), vec3(4, 5, 6), vec3(7, 8, 9));
	mat3 n3(vec3(1, 0, 3), vec3(4, 5, 6), vec3(7, 8, 9));
	mat3 o3(vec3(1, 2, 3), vec3(4, 5, 0), vec3(7, 8, 9));
	mat3 p3(vec3(1, 2, 3), vec3(4, 5, 6), vec3(7, 0, 9));
	CHECK(m3 == m3);
	CHECK(m3 != n3);
	CHECK(m3 != o3);
	CHECK(m3 != p3);

	imat3 i3(ivec3(1, 2, 3), ivec3(4, 5, 6), ivec3(7, 8, 9));
	imat3 j3(ivec3(1, 2, 3), ivec3(4, 5, 6), ivec3(7, 0, 9));
	imat3 k3(ivec3(1, 2, 3), ivec3(4, 5, 0), ivec3(7, 8, 9));
	imat3 l3(ivec3(0, 2, 3), ivec3(4, 5, 6), ivec3(7, 8, 9));
	CHECK(i3 == i3);
	CHECK(i3 != j3);
	CHECK(i3 != k3);
	CHECK(i3 != l3);

	mat4 m4(vec4(1, 2, 3, 4), vec4(3, 4, 5, 6), vec4(5, 6, 7, 8), vec4(7, 8, 9, 0));
	mat4 n4(vec4(1, 2, 0, 4), vec4(3, 4, 5, 6), vec4(5, 6, 7, 8), vec4(7, 8, 9, 0));
	mat4 o4(vec4(1, 2, 3, 4), vec4(0, 4, 5, 6), vec4(5, 6, 7, 8), vec4(7, 8, 9, 0));
	mat4 p4(vec4(1, 2, 3, 4), vec4(3, 4, 5, 6), vec4(5, 0, 7, 8), vec4(7, 8, 9, 0));
	mat4 q4(vec4(1, 2, 3, 4), vec4(3, 4, 5, 6), vec4(5, 6, 7, 8), vec4(7, 8, 9, 1));
	CHECK(m4 == m4);
	CHECK(m4 != n4);
	CHECK(m4 != o4);
	CHECK(m4 != p4);
	CHECK(m4 != q4);

	imat4 i4(ivec4(1, 2, 3, 4), ivec4(3, 4, 5, 6), ivec4(5, 6, 7, 8), ivec4(7, 8, 9, 0));
	imat4 j4(ivec4(1, 0, 3, 4), ivec4(3, 4, 5, 6), ivec4(5, 6, 7, 8), ivec4(7, 8, 9, 0));
	imat4 k4(ivec4(1, 2, 3, 4), ivec4(3, 4, 0, 6), ivec4(5, 6, 7, 8), ivec4(7, 8, 9, 0));
	imat4 l4(ivec4(1, 2, 3, 4), ivec4(3, 4, 5, 6), ivec4(5, 6, 7, 0), ivec4(7, 8, 9, 0));
	imat4 h4(ivec4(1, 2, 3, 4), ivec4(3, 4, 5, 6), ivec4(5, 6, 7, 8), ivec4(0, 8, 9, 0));
	CHECK(i4 == i4);
	CHECK(i4 != j4);
	CHECK(i4 != k4);
	CHECK(i4 != l4);
	CHECK(i4 != h4);

	mat32 m32(vec3(2, 3, 4), vec3(5, 6, 7));
	mat32 n32(vec3(2, 3, 0), vec3(5, 6, 7));
	mat32 o32(vec3(2, 3, 4), vec3(0, 6, 7));
	CHECK(m32 == m32);
	CHECK(m32 != n32);
	CHECK(m32 != o32);

	mat23 m23(vec2(2, 3), vec2(4, 5), vec2(6, 7));
	mat23 n23(vec2(0, 3), vec2(4, 5), vec2(6, 7));
	mat23 o23(vec2(2, 3), vec2(4, 0), vec2(6, 7));
	mat23 p23(vec2(2, 3), vec2(4, 5), vec2(0, 7));
	CHECK(m23 == m23);
	CHECK(m23 != n23);
	CHECK(m23 != o23);
	CHECK(m23 != p23);
}

TEST_CASE("gl_mat: copy constructor, assignment")
{
	SECTION("mat2") {
		mat2 m(vec2(2, 3));
		mat2 n(m);   CHECK(n == m);
		m[1][0] = 9; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
	SECTION("imat2") {
		imat2 m(ivec2(2, 3));
		imat2 n(m);  CHECK(n == m);
		m[0][1] = 9; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
	SECTION("mat3") {
		mat3 m(vec3(3, 4, 5));
		mat3 n(m);   CHECK(n == m);
		m[2][1] = 8; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
	SECTION("imat3") {
		imat3 m(ivec3(3, 4, 5));
		imat3 n(m);  CHECK(n == m);
		m[1][2] = 8; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
	SECTION("mat4") {
		mat4 m(vec4(4, 5, 6, 7));
		mat4 n(m);   CHECK(n == m);
		m[3][1] = 2; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
	SECTION("imat3") {
		imat4 m(ivec4(4, 5, 6, 7));
		imat4 n(m);  CHECK(n == m);
		m[0][1] = 1; CHECK(n != m);
		n = m;       CHECK(n == m);
	}
}

TEST_CASE("gl_mat: construct from larger matrix")
{
	mat4 m4(vec4(1, 2, 3, 4), vec4(4, 5, 6, 7), vec4(7, 8, 9, 1), vec4(1, 0, 0, 0));
	mat3 m3(vec3(1, 2, 3), vec3(4, 5, 6), vec3(7, 8, 9));
	mat2 m2(vec2(1, 2), vec2(4, 5));
	CHECK(mat3(m4) == m3);
	CHECK(mat2(m4) == m2);
	CHECK(mat2(m3) == m2);

	imat4 i4(ivec4(1, 2, 3, 4), ivec4(4, 5, 6, 7), ivec4(7, 8, 9, 1), ivec4(1, 0, 0, 0));
	imat3 i3(ivec3(1, 2, 3), ivec3(4, 5, 6), ivec3(7, 8, 9));
	imat2 i2(ivec2(1, 2), ivec2(4, 5));
	CHECK(imat3(i4) == i3);
	CHECK(imat2(i4) == i2);
	CHECK(imat2(i3) == i2);

	CHECK(mat32(m4) == mat32(vec3(1, 2, 3), vec3(4, 5, 6)));
	CHECK(mat32(m3) == mat32(vec3(1, 2, 3), vec3(4, 5, 6)));
	CHECK(mat23(m4) == mat23(vec2(1, 2), vec2(4, 5), vec2(7, 8)));
	CHECK(mat23(m3) == mat23(vec2(1, 2), vec2(4, 5), vec2(7, 8)));
}

TEST_CASE("gl_mat: addition, subtraction, negation")
{
	mat3 m3(vec3(1, 2, 3), vec3(4, 5, 6), vec3(7, 8, 9));
	mat3 n3(vec3(2, 4, 6), vec3(1, 3, 5), vec3(7, 8, 9));
	mat3 o3 = m3 + n3;
	mat3 p3 = m3 - n3;
	CHECK(o3 == mat3(vec3( 3,  6,  9), vec3(5, 8, 11), vec3(14, 16, 18)));
	CHECK(p3 == mat3(vec3(-1, -2, -3), vec3(3, 2,  1), vec3( 0,  0,  0)));
	p3 += n3;
	o3 -= n3;
	CHECK(o3 == m3);
	CHECK(p3 == m3);
	CHECK(-m3 == mat3(vec3(-1, -2, -3), vec3(-4, -5, -6), vec3(-7, -8, -9)));

	imat3 i3(ivec3(6, 3, 0), ivec3(2, 4, 6), ivec3(3, 4, 5));
	imat3 j3(ivec3(1, 2, 3), ivec3(4, 0, 4), ivec3(0, 1, 1));
	imat3 k3 = i3 + j3;
	imat3 l3 = i3 - j3;
	CHECK(k3 == imat3(ivec3(7, 5,  3), ivec3( 6, 4, 10), ivec3(3, 5, 6)));
	CHECK(l3 == imat3(ivec3(5, 1, -3), ivec3(-2, 4,  2), ivec3(3, 3, 4)));
	l3 += j3;
	k3 -= j3;
	CHECK(k3 == i3);
	CHECK(l3 == i3);
	CHECK(-i3 == imat3(ivec3(-6, -3, -0), ivec3(-2, -4, -6), ivec3(-3, -4, -5)));

	mat4 m4(vec4(1, 2, 3, 4), vec4(4, 5, 6, 7), vec4(7, 8, 9, 1), vec4(1, 0, 0, 0));
	mat4 n4(vec4(2, 4, 6, 8), vec4(1, 3, 5, 7), vec4(7, 8, 9, 2), vec4(0, 0, 0, 1));
	mat4 o4 = m4 + n4;
	mat4 p4 = m4 - n4;
	CHECK(o4 == mat4(vec4( 3,  6,  9, 12), vec4(5, 8, 11, 14), vec4(14, 16, 18,  3), vec4(1, 0, 0,  1)));
	CHECK(p4 == mat4(vec4(-1, -2, -3, -4), vec4(3, 2,  1,  0), vec4( 0,  0,  0, -1), vec4(1, 0, 0, -1)));
	p4 += n4;
	o4 -= n4;
	CHECK(o4 == m4);
	CHECK(p4 == m4);
	CHECK(-m4 == mat4(vec4(-1, -2, -3, -4), vec4(-4, -5, -6, -7), vec4(-7, -8, -9, -1), vec4(-1, 0, 0, 0)));

	imat4 i4(ivec4(0, 1, 2, 3), ivec4(4, 3, 2, 1), ivec4(1, 1, 1, 1), ivec4(5, 5, 4, 4));
	imat4 j4(ivec4(2, 3, 4, 5), ivec4(1, 1, 2, 2), ivec4(0, 0, 1, 2), ivec4(1, 2, 2, 0));
	imat4 k4 = i4 + j4;
	imat4 l4 = i4 - j4;
	CHECK(k4 == imat4(ivec4( 2,  4,  6,  8), ivec4(5, 4, 4,  3), ivec4(1, 1, 2,  3), ivec4(6, 7, 6, 4)));
	CHECK(l4 == imat4(ivec4(-2, -2, -2, -2), ivec4(3, 2, 0, -1), ivec4(1, 1, 0, -1), ivec4(4, 3, 2, 4)));
	l4 += j4;
	k4 -= j4;
	CHECK(k4 == i4);
	CHECK(l4 == i4);
	CHECK(-i4 == imat4(ivec4(0, -1, -2, -3), ivec4(-4, -3, -2, -1), ivec4(-1, -1, -1, -1), ivec4(-5, -5, -4, -4)));

	mat32 m32(vec3(3, 4, 5), vec3(4, 2, 0));
	mat32 n32(vec3(1, 1, 2), vec3(4, 4, 3));
	mat32 o32 = m32 + n32;
	mat32 p32 = m32 - n32;
	CHECK(o32 == mat32(vec3(4, 5, 7), vec3(8,  6,  3)));
	CHECK(p32 == mat32(vec3(2, 3, 3), vec3(0, -2, -3)));
	p32 += n32;
	o32 -= n32;
	CHECK(o32 == m32);
	CHECK(p32 == m32);
	CHECK(-m32 == mat32(vec3(-3, -4, -5), vec3(-4, -2, 0)));
}

TEST_CASE("gl_mat: matrix * scalar")
{
	mat3 m3(vec3(1, 2, 3), vec3(4,  5,  6), vec3( 7,  8,  9));
	mat3 n3(vec3(2, 4, 6), vec3(8, 10, 12), vec3(14, 16, 18));
	CHECK((2.0f * m3) == n3);
	CHECK((m3 * 2.0f) == n3);
	m3 *= 2.0f;
	CHECK(m3 == n3);

	imat3 i3(ivec3(2, 1, 0), ivec3(-1,  5, 3), ivec3( 7, 0, -3));
	imat3 j3(ivec3(4, 2, 0), ivec3(-2, 10, 6), ivec3(14, 0, -6));
	CHECK((2 * i3) == j3);
	CHECK((i3 * 2) == j3);
	i3 *= 2;
	CHECK(i3 == j3);

	mat4 m4(vec4(1, 2, 3, 4), vec4(4,  5,  6,  7), vec4( 7,  8,  9, 10), vec4(0, 0, -1, -2));
	mat4 n4(vec4(2, 4, 6, 8), vec4(8, 10, 12, 14), vec4(14, 16, 18, 20), vec4(0, 0, -2, -4));
	CHECK((2.0f * m4) == n4);
	CHECK((m4 * 2.0f) == n4);
	m4 *= 2.0f;
	CHECK(m4 == n4);

	imat4 i4(ivec4(0, 1, 2, 3), ivec4(1, 0, 0, 2), ivec4(3, 2, 1, 0), ivec4(0, 0, -1, -2));
	imat4 j4(ivec4(0, 3, 6, 9), ivec4(3, 0, 0, 6), ivec4(9, 6, 3, 0), ivec4(0, 0, -3, -6));
	CHECK((3 * i4) == j4);
	CHECK((i4 * 3) == j4);
	i4 *= 3;
	CHECK(i4 == j4);

	mat32 m32(vec3( 3,  4,   5), vec3( 4,  2, 0));
	mat32 n32(vec3(-6, -8, -10), vec3(-8, -4, 0));
	CHECK((-2.0f * m32) == n32);
	CHECK((m32 * -2.0f) == n32);
	m32 *= -2.0f;
	CHECK(m32 == n32);
}

TEST_CASE("gl_mat: matrix * column-vector")
{
	mat3 m3(vec3(1, 2, 3), vec3(4, 5, 6), vec3(7, 8, 9));
	CHECK((m3 * vec3(1, 0, 0)) == vec3( 1,  2,  3));
	CHECK((m3 * vec3(0, 2, 0)) == vec3( 8, 10, 12));
	CHECK((m3 * vec3(0, 0, 3)) == vec3(21, 24, 27));
	CHECK((m3 * vec3(1, 2, 3)) == vec3(30, 36, 42));

	imat3 i3(ivec3(0, -2, 1), ivec3(1, -1, 0), ivec3(-1, -2, 1));
	CHECK((i3 * ivec3(-1, 0, 0)) == ivec3( 0,  2, -1));
	CHECK((i3 * ivec3( 0, 0, 0)) == ivec3( 0,  0,  0));
	CHECK((i3 * ivec3( 0, 0, 3)) == ivec3(-3, -6,  3));
	CHECK((i3 * ivec3(-1, 0, 3)) == ivec3(-3, -4,  2));

	mat4 m4(vec4(1, 2, 3, 4), vec4(4, 5, 6, 7), vec4(7, 8, 9, 1), vec4(0, 1, 0, -1));
	CHECK((m4 * vec4(1, 0, 0, 0)) == vec4( 1,  2,  3,  4));
	CHECK((m4 * vec4(0, 2, 0, 0)) == vec4( 8, 10, 12, 14));
	CHECK((m4 * vec4(0, 0, 3, 0)) == vec4(21, 24, 27,  3));
	CHECK((m4 * vec4(0, 0, 0, 4)) == vec4( 0,  4,  0, -4));
	CHECK((m4 * vec4(1, 2, 3, 4)) == vec4(30, 40, 42, 17));

	imat4 i4(ivec4(-1, 2, 0, -3), ivec4(0, 1, 2, -1), ivec4(0, 1, 1, 0), ivec4(-1, 1, 0, -1));
	CHECK((i4 * ivec4(-1, 0,  0, 0)) == ivec4( 1, -2,  0,  3));
	CHECK((i4 * ivec4( 0, 2,  0, 0)) == ivec4( 0,  2,  4, -2));
	CHECK((i4 * ivec4( 0, 0, -2, 0)) == ivec4( 0, -2, -2,  0));
	CHECK((i4 * ivec4( 0, 0,  0, 1)) == ivec4(-1,  1,  0, -1));
	CHECK((i4 * ivec4(-1, 2, -2, 1)) == ivec4( 0, -1,  2,  0));

	mat32 m32(vec3(3, 4, 5), vec3(4, 2, 0));
	CHECK((m32 * vec2(2, 3)) == vec3(18, 14, 10));

	mat23 m23(vec2(3, 4), vec2(1, 5), vec2(4, 2));
	CHECK((m23 * vec3(1, 2, 3)) == vec2(17, 20));
}

TEST_CASE("gl_mat: matrix * matrix")
{
	mat3 m3(vec3(1, -1,  1), vec3(2,  0, -1), vec3(0, 1, 1));
	mat3 n3(vec3(1,  1, -1), vec3(0, -2,  1), vec3(1, 0, 1));
	mat3 o3(vec3(0, -1,  2), vec3(1,  2, -2), vec3(0, 2, 1));
	CHECK((m3 * n3) == mat3(vec3(3, -2, -1), vec3(-4,  1,  3), vec3(1,  0, 2)));
	CHECK((n3 * o3) == mat3(vec3(2,  2,  1), vec3(-1, -3, -1), vec3(1, -4, 3)));
	CHECK(((m3 * n3) * o3) == (m3 * (n3 * o3)));

	imat3 i3(ivec3( 1, 1,  1), ivec3(-2,  0, -1), ivec3(0, -1, 1));
	imat3 j3(ivec3(-1, 0, -1), ivec3( 1, -2,  1), ivec3(1, -1, 1));
	CHECK((i3 * j3) == imat3(ivec3(-1, 0, -2), ivec3(5, 0, 4), ivec3(3, 0, 3)));

	mat4 m4(vec4(1, -1,  1, 0), vec4(2,  0, -1, 1), vec4(0, 1, 1, -1), vec4(2, 0, 1, -1));
	mat4 n4(vec4(1,  1, -1, 1), vec4(0, -2,  1, 0), vec4(1, 0, 1, -1), vec4(0, 1, 2, -2));
	CHECK((m4 * n4) == mat4(vec4(5, -2, 0, 1), vec4(-4, 1,  3, -3), vec4(-1,  0, 1, 0), vec4(-2, 2, -1, 1)));
	CHECK((n4 * m4) == mat4(vec4(2, 3, -1, 0), vec4( 1, 3, -1,  1), vec4( 1, -3, 0, 1), vec4( 3, 1, -3, 3)));

	imat4 i4(ivec4(1, -1,  1, 2), ivec4(2, -1, -1,  1), ivec4(2,  1, 1, -1), ivec4( 2, -1, 1, -1));
	imat4 j4(ivec4(1,  1, -1, 1), ivec4(2, -2,  1, -1), ivec4(1, -2, 1, -1), ivec4(-1,  1, 2, -2));
	CHECK((i4 * j4) == imat4(ivec4(3, -4, 0, 3), ivec4(-2, 2, 4, 2), ivec4(-3, 3, 3, 0), ivec4(1, 4, -2, -1)));

	mat32 m32(vec3(1, 2, -1), vec3(-1, -1, 2));
	mat23 m23(vec2(1, -2), vec2(2, -1), vec2(1, -1));
	CHECK((m32 * m23) == mat3(vec3(3, 4, -5), vec3(3, 5, -4), vec3(2, 3, -3)));
	CHECK((m23 * m32) == mat2(vec2(4, -3), vec2(-1, 1)));
	CHECK((m3  * m32) == mat32(vec3(5, -2, -2), vec3(-3, 3, 2)));
	CHECK((m23 * m3 ) == mat23(vec2(0, -2), vec2(1, -3), vec2(3, -2)));
}

TEST_CASE("gl_mat: transpose")
{
	mat3 m3(vec3(1, -1, 1), vec3(2, 0, -1), vec3(0, 1, 1));
	CHECK(transpose(m3) == mat3(vec3(1, 2, 0), vec3(-1, 0, 1), vec3(1, -1, 1)));

	imat3 i3(ivec3(1, 1, 1), ivec3(-2, 0, -1), ivec3(0, -1, 1));
	CHECK(transpose(i3) == imat3(ivec3(1, -2, 0), ivec3(1, 0, -1), ivec3(1, -1, 1)));

	mat4 m4(vec4(1, -1, 1, 0), vec4(2, 0, -1, 1), vec4(0, 1, 1, -1), vec4(2, 0, 1, -1));
	CHECK(transpose(m4) == mat4(vec4(1, 2, 0, 2), vec4(-1, 0, 1, 0), vec4(1, -1, 1, 1), vec4(0, 1, -1, -1)));

	imat4 i4(ivec4(1, -1, 1, 2), ivec4(2, -1, -1, 1), ivec4(2, 1, 1, -1), ivec4(2, -1, 1, -1));
	CHECK(transpose(i4) == imat4(ivec4(1, 2, 2, 2), ivec4(-1, -1, 1, -1), ivec4(1, -1, 1, 1), ivec4(2, 1, -1, -1)));

	mat32 m32(vec3(1, 2, 3), vec3(4, 5, 6));
	mat23 m23(vec2(1, 4), vec2(2, 5), vec2(3, 6));
	CHECK(transpose(m32) == m23);
	CHECK(transpose(m23) == m32);
}

TEST_CASE("gl_mat: determinant, inverse")
{
	mat2 m2(vec2(1, -1), vec2(0, 1));
	CHECK(determinant(m2) == 1.0f);
	mat2 i2 = inverse(m2);
	CHECK(determinant(i2) == 1.0f);
	CHECK(i2 == mat2(vec2(1, 1), vec2(0, 1)));
	CHECK(m2 * i2 == mat2());
	CHECK(i2 * m2 == mat2());

	mat3 m3(vec3(1, 0, -1), vec3(0, 1, 0), vec3(1, -1, 1));
	CHECK(determinant(m3) == 2.0f);
	mat3 i3 = inverse(m3);
	CHECK(determinant(i3) == 0.5f);
	CHECK(i3 == mat3(vec3(0.5f, 0.5f, 0.5f), vec3(0, 1, 0), vec3(-0.5f, 0.5f, 0.5f)));
	CHECK(m3 * i3 == mat3());
	CHECK(i3 * m3 == mat3());

	mat4 m4(vec4(0, 1, 1, 0), vec4(-1, 0, 1, 0), vec4(1, 1, 0, -1), vec4(0, 1, 0, 0));
	CHECK(determinant(m4) == -1.0f);
	mat4 i4 = inverse(m4);
	CHECK(approxEq(i4, mat4(vec4(1, -1, 0, -1), vec4(0, 0, 0, 1), vec4(1, 0, 0, -1), vec4(1, -1, -1, 0))));
	CHECK(approxEq(m4 * i4, mat4()));
	CHECK(approxEq(i4 * m4, mat4()));
}

TEST_CASE("gl_mat: norm-2 squared")
{
	mat3 m3(vec3(1, -1, 1), vec3(2, 0, -1), vec3(0, 1, 1));
	CHECK(norm2_2(m3) == 10);

	imat3 i3(ivec3(1, 2, 1), ivec3(-2, 3, -1), ivec3(2, 1, 4));
	CHECK(norm2_2(i3) == 41);

	mat4 m4(vec4(1, -1, 1, 2), vec4(2, 0, -1, 1), vec4(0, 1, 1, 0), vec4(0, 1, 1, 3));
	CHECK(norm2_2(m4) == 26);

	imat4 i4(ivec4(1, 2, 1, -3), ivec4(-2, 3, -1, 0), ivec4(2, 1, 4, -2), ivec4(0, 2, 2, 2));
	CHECK(norm2_2(i4) == 66);

	mat32 m32(vec3(1, 2, -1), vec3(-1, -1, 2));
	mat23 m23(vec2(1, -1), vec2(2, -1), vec2(-1, 2));
	CHECK(norm2_2(m32) == norm2_2(m23));
}


#if 0

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

#endif
