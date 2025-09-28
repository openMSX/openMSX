#include "catch.hpp"
#include "gl_vec.hh"

#include "Math.hh"

#include <concepts>

using namespace gl;

// Test approximations.
template<std::floating_point T>
bool approxEq(T x, T y)
{
	return std::abs(x - y) < T(1.0e-5);
}
template<int N, std::floating_point T>
bool approxEq(const vecN<N, T>& x, const vecN<N, T>&y)
{
	return length2(x - y) < T(1.0e-4);
}


TEST_CASE("gl_vec: rsqrt")
{
	CHECK(gl::rsqrt(16.0f) == 0.25f);
	CHECK(gl::rsqrt(16.0 ) == 0.25 );
}

TEST_CASE("gl_vec: radians, degrees")
{
	CHECK(approxEq(radians(  0.0), 0.0         ));
	CHECK(approxEq(radians( 90.0), Math::pi / 2));
	CHECK(approxEq(radians(180.0), Math::pi    ));
	CHECK(approxEq(degrees(0.0         ),   0.0));
	CHECK(approxEq(degrees(Math::pi / 2),  90.0));
	CHECK(approxEq(degrees(Math::pi    ), 180.0));
}

// It's useful to test both integer and float variants because the
// former are implemented in plain C++ and (only) the latter have SSE
// optimizations.

TEST_CASE("gl_vec: constructors")
{
	SECTION("default constructor") {
		vec2 v2;
		CHECK(v2[0] == 0);
		CHECK(v2[1] == 0);
		ivec2 i2;
		CHECK(i2[0] == 0);
		CHECK(i2[1] == 0);

		vec3 v3;
		CHECK(v3[0] == 0);
		CHECK(v3[1] == 0);
		CHECK(v3[2] == 0);
		ivec3 i3;
		CHECK(i3[0] == 0);
		CHECK(i3[1] == 0);
		CHECK(i3[2] == 0);

		vec4 v4;
		CHECK(v4[0] == 0);
		CHECK(v4[1] == 0);
		CHECK(v4[2] == 0);
		CHECK(v4[3] == 0);
		ivec4 i4;
		CHECK(i4[0] == 0);
		CHECK(i4[1] == 0);
		CHECK(i4[2] == 0);
		CHECK(i4[3] == 0);
	}
	SECTION("broadcast constructor") {
		vec2 v2(2);
		CHECK(v2[0] == 2);
		CHECK(v2[1] == 2);
		ivec2 i2(2);
		CHECK(i2[0] == 2);
		CHECK(i2[1] == 2);

		vec3 v3(3);
		CHECK(v3[0] == 3);
		CHECK(v3[1] == 3);
		CHECK(v3[2] == 3);
		ivec3 i3(3);
		CHECK(i3[0] == 3);
		CHECK(i3[1] == 3);
		CHECK(i3[2] == 3);

		vec4 v4(4);
		CHECK(v4[0] == 4);
		CHECK(v4[1] == 4);
		CHECK(v4[2] == 4);
		CHECK(v4[3] == 4);
		ivec4 i4(4);
		CHECK(i4[0] == 4);
		CHECK(i4[1] == 4);
		CHECK(i4[2] == 4);
		CHECK(i4[3] == 4);
	}
	SECTION("per-element constructor") {
		vec2 v2(2, 3);
		CHECK(v2[0] == 2);
		CHECK(v2[1] == 3);
		ivec2 i2(2, 3);
		CHECK(i2[0] == 2);
		CHECK(i2[1] == 3);

		vec3 v3(3, 4, 5);
		CHECK(v3[0] == 3);
		CHECK(v3[1] == 4);
		CHECK(v3[2] == 5);
		ivec3 i3(3, 4, 5);
		CHECK(i3[0] == 3);
		CHECK(i3[1] == 4);
		CHECK(i3[2] == 5);

		vec4 v4(4, 5, 6, 7);
		CHECK(v4[0] == 4);
		CHECK(v4[1] == 5);
		CHECK(v4[2] == 6);
		CHECK(v4[3] == 7);
		ivec4 i4(4, 5, 6, 7);
		CHECK(i4[0] == 4);
		CHECK(i4[1] == 5);
		CHECK(i4[2] == 6);
		CHECK(i4[3] == 7);
	}
	SECTION("construct from smaller vectors/scalar") {
		vec3 v3(3, vec2(4, 5));
		CHECK(v3[0] == 3);
		CHECK(v3[1] == 4);
		CHECK(v3[2] == 5);
		ivec3 i3(ivec2(4, 5), 3);
		CHECK(i3[0] == 4);
		CHECK(i3[1] == 5);
		CHECK(i3[2] == 3);

		vec4 v4(4, vec3(5, 6, 7));
		CHECK(v4[0] == 4);
		CHECK(v4[1] == 5);
		CHECK(v4[2] == 6);
		CHECK(v4[3] == 7);
		ivec4 i4(ivec3(5, 6, 7), 4);
		CHECK(i4[0] == 5);
		CHECK(i4[1] == 6);
		CHECK(i4[2] == 7);
		CHECK(i4[3] == 4);

		vec4 w4(vec2(5, 6), vec2(7, 8));
		CHECK(w4[0] == 5);
		CHECK(w4[1] == 6);
		CHECK(w4[2] == 7);
		CHECK(w4[3] == 8);
		ivec4 j4(ivec2(5, 6), ivec2(7, 8));
		CHECK(j4[0] == 5);
		CHECK(j4[1] == 6);
		CHECK(j4[2] == 7);
		CHECK(j4[3] == 8);

	#if 0
		// Not implemented yet (also not yet needed).
		vec4 x4(1, vec2(3, 5), 7);
		CHECK(x4[0] == 1);
		CHECK(x4[1] == 3);
		CHECK(x4[2] == 5);
		CHECK(x4[3] == 7);
		ivec4 k4(2, ivec2(4, 6), 8);
		CHECK(k4[0] == 2);
		CHECK(k4[1] == 4);
		CHECK(k4[2] == 6);
		CHECK(k4[3] == 8);
	#endif

	#if 0
		// Ok, compile error:
		//   static assertion failed: wrong vector length in constructor
		ivec3 wrong1(3, ivec3(4, 5, 6));
		ivec4 wrong2(3, ivec2(4, 5));
		ivec4 wrong3(ivec2(4, 5), ivec3(8, 9));
	#endif
	}
}

TEST_CASE("gl_vec: modify elements")
{
	vec2 v2;
	v2[0] = 5;
	CHECK(v2[0] == 5);
	CHECK(v2[1] == 0);
	v2[1] = 7;
	CHECK(v2[0] == 5);
	CHECK(v2[1] == 7);
	ivec2 i2;
	i2[0] = 5;
	CHECK(i2[0] == 5);
	CHECK(i2[1] == 0);
	i2[1] = 7;
	CHECK(i2[0] == 5);
	CHECK(i2[1] == 7);

	vec3 v3(1);
	v3[1] = 2;
	CHECK(v3[0] == 1);
	CHECK(v3[1] == 2);
	CHECK(v3[2] == 1);
	ivec3 i3(1);
	i3[1] = 2;
	CHECK(i3[0] == 1);
	CHECK(i3[1] == 2);
	CHECK(i3[2] == 1);

	vec4 v4(1, 2, 3, 4);
	v4[0] = 9; v4[2] = 8;
	CHECK(v4[0] == 9);
	CHECK(v4[1] == 2);
	CHECK(v4[2] == 8);
	CHECK(v4[3] == 4);
	ivec4 i4(1, 2, 3, 4);
	i4[0] = 9; i4[2] = 8;
	CHECK(i4[0] == 9);
	CHECK(i4[1] == 2);
	CHECK(i4[2] == 8);
	CHECK(i4[3] == 4);
}

TEST_CASE("gl_vec: (in)equality")
{
	CHECK( vec2(1, 2) ==  vec2(1, 2));
	CHECK( vec2(1, 2) !=  vec2(1, 4));
	CHECK( vec2(1, 2) !=  vec2(3, 2));
	CHECK( vec2(1, 2) !=  vec2(3, 4));
	CHECK(ivec2(1, 2) == ivec2(1, 2));
	CHECK(ivec2(1, 2) != ivec2(1, 4));
	CHECK(ivec2(1, 2) != ivec2(3, 2));
	CHECK(ivec2(1, 2) != ivec2(3, 4));

	CHECK( vec3(1, 2, 3) ==  vec3(1, 2, 3));
	CHECK( vec3(1, 2, 3) !=  vec3(1, 2, 4));
	CHECK(ivec3(1, 2, 3) == ivec3(1, 2, 3));
	CHECK(ivec3(1, 2, 3) != ivec3(1, 2, 4));

	CHECK( vec4(1, 2, 3, 4) ==  vec4(1, 2, 3, 4));
	CHECK( vec4(1, 2, 3, 4) !=  vec4(1, 2, 4, 4));
	CHECK(ivec4(1, 2, 3, 4) == ivec4(1, 2, 3, 4));
	CHECK(ivec4(1, 2, 3, 4) != ivec4(1, 2, 4, 4));
}

TEST_CASE("gl_vec: copy constructor, assignment")
{
	SECTION("vec2") {
		vec2 v(2, 3);
		vec2 w(v); CHECK(v == w);
		v[0] = 9;  CHECK(v != w);
		w = v;     CHECK(v == w);
	}
	SECTION("ivec2") {
		ivec2 v(2, 3);
		ivec2 w(v); CHECK(v == w);
		v[1] = 9;   CHECK(v != w);
		w = v;      CHECK(v == w);
	}
	SECTION("vec3") {
		vec3 v(3, 4, 5);
		vec3 w(v); CHECK(v == w);
		v[2] = 8;  CHECK(v != w);
		w = v;     CHECK(v == w);
	}
	SECTION("ivec3") {
		ivec3 v(3, 4, 5);
		ivec3 w(v); CHECK(v == w);
		v[1] = 8;   CHECK(v != w);
		w = v;      CHECK(v == w);
	}
	SECTION("vec4") {
		vec4 v(4, 5, 6, 7);
		vec4 w(v); CHECK(v == w);
		v[3] = 0;  CHECK(v != w);
		w = v;     CHECK(v == w);
	}
	SECTION("ivec4") {
		ivec4 v(4, 5, 6, 7);
		ivec4 w(v); CHECK(v == w);
		v[0] = 1;   CHECK(v != w);
		w = v;      CHECK(v == w);
	}
}

TEST_CASE("gl_vec: construct from larger vector")
{
	vec4 v4(1, 2, 3, 4);
	vec3 v3(7, 8, 9);
	CHECK(vec3(v4) == vec3(1, 2, 3));
	CHECK(vec2(v4) == vec2(1, 2));
	CHECK(vec2(v3) == vec2(7, 8));

	ivec4 i4(1, 2, 3, 4);
	ivec3 i3(7, 8, 9);
	CHECK(ivec3(i4) == ivec3(1, 2, 3));
	CHECK(ivec2(i4) == ivec2(1, 2));
	CHECK(ivec2(i3) == ivec2(7, 8));
}

// From here on I'll skip tests on vec2 and ivec2.
// Vectors of dimension 2 and 3 are handled by the same C++ code, so
// if there's a bug it should show up in both types.

TEST_CASE("gl_vec: vector add")
{
	 vec3 v3(1, 2, 3);  vec4 v4(1, 2, 3, 4);
	ivec3 i3(1, 2, 3); ivec4 i4(1, 2, 3, 4);
	CHECK((v3 +  vec3(4, 5, 6))    ==  vec3(5, 7, 9));
	CHECK((i3 + ivec3(4, 5, 6))    == ivec3(5, 7, 9));
	CHECK((v4 +  vec4(4, 5, 6, 7)) ==  vec4(5, 7, 9, 11));
	CHECK((i4 + ivec4(4, 5, 6, 7)) == ivec4(5, 7, 9, 11));

	v3 +=  vec3(2, 3, 4);
	i3 += ivec3(2, 3, 4);
	v4 +=  vec4(2, 3, 4, 5);
	i4 += ivec4(2, 3, 4, 5);
	CHECK(v3 ==  vec3(3, 5, 7));
	CHECK(i3 == ivec3(3, 5, 7));
	CHECK(v4 ==  vec4(3, 5, 7, 9));
	CHECK(i4 == ivec4(3, 5, 7, 9));
}

TEST_CASE("gl_vec: vector subtract")
{
	 vec3 v3(1, 2, 3);  vec4 v4(1, 2, 3, 4);
	ivec3 i3(1, 2, 3); ivec4 i4(1, 2, 3, 4);
	CHECK((v3 -  vec3(4, 3, 2))    ==  vec3(-3, -1, 1));
	CHECK((i3 - ivec3(4, 3, 2))    == ivec3(-3, -1, 1));
	CHECK((v4 -  vec4(4, 3, 2, 1)) ==  vec4(-3, -1, 1, 3));
	CHECK((i4 - ivec4(4, 3, 2, 1)) == ivec4(-3, -1, 1, 3));

	v3 -=  vec3(2, 4, 6);
	i3 -= ivec3(2, 4, 6);
	v4 -=  vec4(2, 4, 6, 8);
	i4 -= ivec4(2, 4, 6, 8);
	CHECK(v3 ==  vec3(-1, -2, -3));
	CHECK(i3 == ivec3(-1, -2, -3));
	CHECK(v4 ==  vec4(-1, -2, -3, -4));
	CHECK(i4 == ivec4(-1, -2, -3, -4));
}

TEST_CASE("gl_vec: vector negate")
{
	CHECK((- vec3(4, -3, 2))     ==  vec3(-4, 3, -2));
	CHECK((-ivec3(4, -3, 2))     == ivec3(-4, 3, -2));
	CHECK((- vec4(4, -3, 2, -1)) ==  vec4(-4, 3, -2, 1));
	CHECK((-ivec4(4, -3, 2, -1)) == ivec4(-4, 3, -2, 1));
}

TEST_CASE("gl_vec: component-wise vector multiplication")
{
	 vec3 v3(0, 2, -3);  vec4 v4(0, 2, -3, 4);
	ivec3 i3(0, 2, -3); ivec4 i4(0, 2, -3, 4);
	// scalar * vector
	CHECK((2.0f * v3) ==  vec3(0, 4, -6));
	CHECK((2    * i3) == ivec3(0, 4, -6));
	CHECK((2.0f * v4) ==  vec4(0, 4, -6, 8));
	CHECK((2    * i4) == ivec4(0, 4, -6, 8));
	// vector * scalar
	CHECK((v3 * 2.0f) ==  vec3(0, 4, -6));
	CHECK((i3 * 2   ) == ivec3(0, 4, -6));
	CHECK((v4 * 2.0f) ==  vec4(0, 4, -6, 8));
	CHECK((i4 * 2   ) == ivec4(0, 4, -6, 8));
	// vector * vector
	 vec3 w3(-1, 2, -3);  vec4 w4(-1, 2, -3, -4);
	ivec3 j3(-1, 2, -3); ivec4 j4(-1, 2, -3, -4);
	CHECK((v3 * w3) ==  vec3(0, 4, 9));
	CHECK((i3 * j3) == ivec3(0, 4, 9));
	CHECK((v4 * w4) ==  vec4(0, 4, 9, -16));
	CHECK((i4 * j4) == ivec4(0, 4, 9, -16));
	// *= scalar
	v3 *= 2.0f; CHECK(v3 ==  vec3(0, 4, -6));
	i3 *= 2;    CHECK(i3 == ivec3(0, 4, -6));
	v4 *= 2.0f; CHECK(v4 ==  vec4(0, 4, -6, 8));
	i4 *= 2;    CHECK(i4 == ivec4(0, 4, -6, 8));
	// *= vector
	v3 *= w3; CHECK(v3 ==  vec3(0, 8, 18));
	i3 *= j3; CHECK(i3 == ivec3(0, 8, 18));
	v4 *= w4; CHECK(v4 ==  vec4(0, 8, 18, -32));
	i4 *= j4; CHECK(i4 == ivec4(0, 8, 18, -32));
}

TEST_CASE("gl_vec: reciprocal (only floating point)")
{
	CHECK(approxEq(recip(vec3(4, 2, 0.5)),    vec3(0.25, 0.5, 2)));
	CHECK(approxEq(recip(vec4(4, 2, 0.5, 1)), vec4(0.25, 0.5, 2, 1)));
}

TEST_CASE("gl_vec: component-wise division (only floating point)")
{
	vec3 v3( 0,  1, -4);
	vec3 w3(-1, -2,  2);
	CHECK((v3 / 2.0f) == vec3( 0,  0.5f, -2));
	CHECK((6.0f / w3) == vec3(-6, -3, 3));
	CHECK((v3 / w3)   == vec3( 0, -0.5f, -2));

	vec4 v4( 2, 1, -4, -3);
	vec4 w4(-1, 2,  2, -6);
	CHECK(approxEq((v4 / -2.0f), vec4(-1, -0.5f, 2, 1.5f)));
	CHECK(approxEq((6.0f / w4), vec4(-6, 3, 3, -1)));
	CHECK(approxEq((v4 / w4), vec4(-2, 0.5f, -2, 0.5f)));
}

TEST_CASE("gl_vec: component-wise min/max")
{
	CHECK(min( vec3(2, 3, 4),  vec3(1, -5, 7)) ==  vec3(1, -5, 4));
	CHECK(min(ivec3(2, 3, 4), ivec3(1, -5, 7)) == ivec3(1, -5, 4));
	CHECK(min( vec4(1, -2, 5, -7),  vec4(0, 2, -4, -3)) ==  vec4(0, -2, -4, -7));
	CHECK(min(ivec4(1, -2, 5, -7), ivec4(0, 2, -4, -3)) == ivec4(0, -2, -4, -7));

	CHECK(max( vec3(2, 3, 4),  vec3(1, -5, 7)) ==  vec3(2, 3, 7));
	CHECK(max(ivec3(2, 3, 4), ivec3(1, -5, 7)) == ivec3(2, 3, 7));
	CHECK(max( vec4(1, -2, 5, -7),  vec4(0, 2, -4, -3)) ==  vec4(1, 2, 5, -3));
	CHECK(max(ivec4(1, -2, 5, -7), ivec4(0, 2, -4, -3)) == ivec4(1, 2, 5, -3));
}

TEST_CASE("gl_vec: minimum component within a vector")
{
	CHECK(min_component( vec3(1, 5, -7.2f)) == -7.2f);
	CHECK(min_component(ivec3(3, 2, 4)) == 2);
	CHECK(min_component( vec4(-1, 2, 5.2f, 0)) == -1);
	CHECK(min_component(ivec4(1, -2, 5, -7)) == -7);
}

TEST_CASE("gl_vec: clamp") {
	CHECK(clamp( vec3(2, 3, 4),  vec3(0, 4, -4),  vec3(4, 7, 0)) ==  vec3(2, 4, 0));
	CHECK(clamp(ivec3(2, 3, 4), ivec3(0, 4, -4), ivec3(4, 7, 0)) == ivec3(2, 4, 0));
	CHECK(clamp( vec4(4, 2, 7, 1),  vec4(0, 3, 2, 1),  vec4(4, 6, 8, 3)) ==  vec4(4, 3, 7, 1));
	CHECK(clamp(ivec4(4, 2, 7, 1), ivec4(0, 3, 2, 1), ivec4(4, 6, 8, 3)) == ivec4(4, 3, 7, 1));

	CHECK(clamp( vec3(2, 3, 4), 1.0f, 3.0f) ==  vec3(2, 3, 3));
	CHECK(clamp(ivec3(2, 3, 4), 1,    3   ) == ivec3(2, 3, 3));
	CHECK(clamp( vec4(4, 2, 7, 1), 2.0f, 6.0f) ==  vec4(4, 2, 6, 2));
	CHECK(clamp(ivec4(4, 2, 7, 1), 2,    6   ) == ivec4(4, 2, 6, 2));
}

TEST_CASE("gl_vec: sum of vector components")
{
	CHECK(sum( vec3(4, -3, 2))     == 3.0f);
	CHECK(sum(ivec3(4, -3, 2))     == 3   );
	CHECK(sum( vec4(4, -3, 2, -1)) == 2.0f);
	CHECK(sum(ivec4(4, -3, 2, -1)) == 2   );

	CHECK(sum_broadcast(vec3(4, -3, 2))     == vec3(3.0f));
	CHECK(sum_broadcast(vec4(4, -3, 2, -1)) == vec4(2.0f));
}

TEST_CASE("gl_vec: dot product")
{
	CHECK(dot( vec3(4, -3, 2),      vec3(-1, 3, 2)    ) == -9.0f);
	CHECK(dot(ivec3(4, -3, 2),     ivec3(-1, 3, 2)    ) == -9   );
	CHECK(dot( vec4(4, -3, 2, -1),  vec4(-1, 3, 2, -2)) == -7.0f);
	CHECK(dot(ivec4(4, -3, 2, -1), ivec4(-1, 3, 2, -2)) == -7   );

	CHECK(dot_broadcast(vec3(4, -3, 2),     vec3(-1, 3, 2)    ) == vec3(-9.0f));
	CHECK(dot_broadcast(vec4(4, -3, 2, -1), vec4(-1, 3, 2, -2)) == vec4(-7.0f));
}

TEST_CASE("gl_vec: cross product (only for vectors of length 3)")
{
	CHECK(cross( vec3(4, -3, 2),  vec3(-1, 3, 2)) ==  vec3(-12, -10, 9));
	CHECK(cross(ivec3(4, -3, 2), ivec3(-1, 3, 2)) == ivec3(-12, -10, 9));
}

TEST_CASE("gl_vec: vector length squared (2-norm squared)")
{
	CHECK(length2( vec3(4, -3, 2    )) == 29.0f);
	CHECK(length2(ivec3(4, -3, 2    )) == 29   );
	CHECK(length2( vec4(4, -3, 2, -1)) == 30.0f);
	CHECK(length2(ivec4(4, -3, 2, -1)) == 30   );
}
TEST_CASE("gl_vec: vector length (2-norm), (only floating point)")
{
	CHECK(length(vec3(4, -3, 2    )) == sqrtf(29.0f));
	CHECK(length(vec4(4, -3, 2, -1)) == sqrtf(30.0f));
}

TEST_CASE("gl_vec: vector normalization, only floating point")
{
	CHECK(normalize(vec3( 0, 4, -3   )) == vec3( 0.0f, 0.8f, -0.6f));
	CHECK(normalize(vec4(-4, 0,  0, 3)) == vec4(-0.8f, 0.0f,  0.0f, 0.6f));
}

TEST_CASE("gl_vec: round")
{
	CHECK(round(vec3( 1.1f, 2.6f, -3.8f))        == ivec3( 1, 3, -4));
	CHECK(round(vec4(-1.1f, 2.4f,  3.8f, -4.6f)) == ivec4(-1, 2,  4, -5));
	// round integers, nop
	CHECK(round(ivec4(1, -2, 3, -4)) == ivec4(1, -2, 3, -4));
}

TEST_CASE("gl_vec: trunc")
{
	CHECK(trunc(vec3( 1.1f, 2.5f, -3.8f))        == ivec3( 1, 2, -3));
	CHECK(trunc(vec4(-1.1f, 2.5f,  3.8f, -4.5f)) == ivec4(-1, 2,  3, -4));
	// trunc integers, nop
	CHECK(trunc(ivec4(1, -2, 3, -4)) == ivec4(1, -2, 3, -4));
}

TEST_CASE("gl_vec: named elements")
{
	SECTION("ivec2") {
		ivec2 v2(10, 20);
		CHECK(sizeof(v2) == 2 * sizeof(int));

		CHECK(v2[0] == 10);
		CHECK(v2[1] == 20);
		CHECK(v2.x == 10);
		CHECK(v2.y == 20);

		v2.x = 100;
		v2.y = 200;
		CHECK(v2[0] == 100);
		CHECK(v2[1] == 200);
		CHECK(v2.x == 100);
		CHECK(v2.y == 200);

		v2[0] = 3;
		v2[1] = 4;
		CHECK(v2[0] == 3);
		CHECK(v2[1] == 4);
		CHECK(v2.x == 3);
		CHECK(v2.y == 4);

		//v2.z = 10;     // Ok, compile error
		//int i = v2.z;
	}
	SECTION("ivec3") {
		ivec3 v3(10, 20, 30);
		CHECK(sizeof(v3) == 3 * sizeof(int));

		CHECK(v3.x == 10);
		CHECK(v3.y == 20);
		CHECK(v3.z == 30);

		v3.x = 100;
		v3.y = 200;
		v3.z = 300;
		CHECK(v3.x == 100);
		CHECK(v3.y == 200);
		CHECK(v3.z == 300);

		//v3.w = 10;     // Ok, compile error
		//int i = v3.w;
	}
	SECTION("ivec4") {
		ivec4 v4(10, 20, 30, 40);
		CHECK(sizeof(v4) == 4 * sizeof(int));

		CHECK(v4.x == 10);
		CHECK(v4.y == 20);
		CHECK(v4.z == 30);
		CHECK(v4.w == 40);

		v4.x = 100;
		v4.y = 200;
		v4.z = 300;
		v4.w = 400;
		CHECK(v4.x == 100);
		CHECK(v4.y == 200);
		CHECK(v4.z == 300);
		CHECK(v4.w == 400);
	}
}


#if 0

// The following functions are not part of the actual test. They get compiled,
// but never executed. I used them to (manually) inspect the quality of the
// generated code. Mostly only for vec4, because the code was only optimized
// for that type.

void test_constr(vec4& z)
{
	z = vec4();
}
void test_constr(float x, vec4& z)
{
	z = vec4(x);
}
void test_constr(float a, float b, float c, float d, vec4& z)
{
	z = vec4(a, b, c, d);
}

void test_change0(float x, vec4& z)
{
	z[0] = x;
}
void test_change2(float x, vec4& z)
{
	z[2] = x;
}

void test_extract0(const vec4& x, float& z)
{
	z = x[0];
}
void test_extract2(const vec4& x, float& z)
{
	z = x[2];
}

bool test_equal(const vec4& x, const vec4& y)
{
	return x == y;
}
bool test_not_equal(const vec4& x, const vec4& y)
{
	return x != y;
}

void test_add(const vec4& x, const vec4& y, vec4& z)
{
	z = x + y;
}
void test_add(vec4& x, const vec4& y)
{
	x += y;
}

void test_negate(const vec4& x, vec4& y)
{
	y = -x;
}

void test_mul(const vec4& x, const vec4& y, vec4& z)
{
	z = x * y;
}
void test_mul(float x, const vec4& y, vec4& z)
{
	z = x * y;
}

void test_div(const vec4& x, const vec4& y, vec4& z)
{
	z = x / y;
}
void test_div(float x, const vec4& y, vec4& z)
{
	z = x / y;
}
void test_div(const vec4& x, float y, vec4& z)
{
	z = x / y;
}

void test_sum(const vec4& x, float& y)
{
	y = sum(x);
}
void test_sum_broadcast(const vec4& x, vec4& y)
{
	y = sum_broadcast(x);
}

void test_dot(const vec4& x, const vec4& y, float& z)
{
	z = dot(x, y);
}
void test_dot_broadcast(const vec4& x, const vec4& y, vec4& z)
{
	z = dot_broadcast(x, y);
}

void test_length2(const vec4& x, float& y)
{
	y = length2(x);
}

void test_length(const vec4& x, float& y)
{
	y = length(x);
}

void test_normalize(const vec4& x, vec4& y)
{
	y = normalize(x);
}

void test_recip(const vec4& x, vec4& y)
{
	y = recip(x);
}


void test_constr(vec3& z)
{
	z = vec3();
}
void test_constr(float x, vec3& z)
{
	z = vec3(x);
}
void test_constr(float a, float b, float c, vec3& z)
{
	z = vec3(a, b, c);
}
void test_constr(vec4 x, vec3& y)
{
	y = vec3(x);
}

void test_change0(float x, vec3& z)
{
	z[0] = x;
}
void test_change2(float x, vec3& z)
{
	z[2] = x;
}

void test_extract0(const vec3& x, float& z)
{
	z = x[0];
}
void test_extract2(const vec3& x, float& z)
{
	z = x[2];
}

bool test_equal(const vec3& x, const vec3& y)
{
	return x == y;
}
bool test_not_equal(const vec3& x, const vec3& y)
{
	return x != y;
}

void test_add(const vec3& x, const vec3& y, vec3& z)
{
	z = x + y;
}
void test_add(vec3& x, const vec3& y)
{
	x += y;
}

void test_negate(const vec3& x, vec3& y)
{
	y = -x;
}

void test_mul(const vec3& x, const vec3& y, vec3& z)
{
	z = x * y;
}
void test_mul(float x, const vec3& y, vec3& z)
{
	z = x * y;
}

void test_div(const vec3& x, const vec3& y, vec3& z)
{
	z = x / y;
}
void test_div(float x, const vec3& y, vec3& z)
{
	z = x / y;
}
void test_div(const vec3& x, float y, vec3& z)
{
	z = x / y;
}

void test_min(const vec3& x, const vec3& y, vec3& z)
{
	z = min(x, y);
}

void test_min(const vec4& x, const vec4& y, vec4& z)
{
	z = min(x, y);
}

void test_clamp(const vec3& x, const vec3& y, const vec3& z, vec3& w)
{
	w = clamp(x, y, z);
}

void test_clamp(const vec4& x, const vec4& y, const vec4& z, vec4& w)
{
	w = clamp(x, y, z);
}

void test_clamp(const vec3& x, float y, float z, vec3& w)
{
	w = clamp(x, y, z);
}

void test_clamp(const vec4& x, float y, float z, vec4& w)
{
	w = clamp(x, y, z);
}

void test_clamp(const vec4& x, vec4& y)
{
	y = clamp(x, 0.0f, 1.0f);
}

void test_sum(const vec3& x, float& y)
{
	y = sum(x);
}

void test_dot(const vec3& x, const vec3& y, float& z)
{
	z = dot(x, y);
}

void test_length2(const vec3& x, float& y)
{
	y = length2(x);
}

void test_length(const vec3& x, float& y)
{
	y = length(x);
}

void test_normalize(const vec3& x, vec3& y)
{
	y = normalize(x);
}

void test_round(const vec4& x, ivec4& y)
{
	y = round(x);
}
void test_trunc(const vec4& x, ivec4& y)
{
	y = trunc(x);
}

#endif
