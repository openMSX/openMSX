#include "catch.hpp"
#include "gl_transform.hh"

using namespace gl;

// Test approximations.
static bool approxEq(float x, float y)
{
	return fabsf(x - y) < 1.0e-5f;
}
static constexpr bool approxEq(const vec4& x, const vec4&y)
{
	return length2(x - y) < 1.0e-4f;
}
static constexpr bool approxEq(const mat4& x, const mat4&y)
{
	return norm2_2(x - y) < 1.0e-3f;
}


TEST_CASE("gl_transform: scale")
{
	mat4 S1 = scale(vec3(1, 2, 3));
	mat4 S2 = scale(S1, vec3(2, 3, 4)); // first this scale, then S1
	CHECK(S1 == mat4(vec4(1, 0, 0, 0), vec4(0, 2, 0, 0), vec4(0, 0,  3, 0), vec4(0, 0, 0, 1)));
	CHECK(S2 == mat4(vec4(2, 0, 0, 0), vec4(0, 6, 0, 0), vec4(0, 0, 12, 0), vec4(0, 0, 0, 1)));

	vec4 p(4, 5, 6, 1); // point
	vec4 d(4, 5, 6, 0); // direction
	CHECK(S1 * p == vec4(4, 10, 18, 1));
	CHECK(S1 * d == vec4(4, 10, 18, 0));
	CHECK(S2 * p == vec4(8, 30, 72, 1));
	CHECK(S2 * d == vec4(8, 30, 72, 0));

	CHECK(S1 * S2 == S2 * S1); // scaling is commutative
	CHECK(approxEq(inverse(S1), scale(vec3(1.0f / 1.0f, 1.0f / 2.0f, 1.0f / 3.0f))));
}

TEST_CASE("gl_transform: translate")
{
	mat4 T1 = translate(    vec3( 1, 2,  3));
	mat4 T2 = translate(T1, vec3(-2, 1, -1)); // first this, then T1
	mat4 T3 = translate(    vec3(-1, 3,  2));

	CHECK(T1 == mat4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4( 1, 2, 3, 1)));
	CHECK(T2 == mat4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(-1, 3, 2, 1)));
	CHECK(T2 == T3);

	vec4 p(4, 5, 6, 1); // point
	vec4 d(4, 5, 6, 0); // direction
	CHECK((T1 * p) == (p + vec4( 1, 2, 3, 0)));
	CHECK((T1 * d) == d);
	CHECK((T2 * p) == (p + vec4(-1, 3, 2, 0)));
	CHECK((T2 * d) == d);

	CHECK(T1 * T2 == T2 * T1); // translation is commutative
	CHECK(approxEq(inverse(T1), translate(vec3(-1, -2, -3))));
}

TEST_CASE("gl_transform: scale + translate")
{
	vec3 s(2, 1, 1);
	vec3 t(1, 1, 0);
	mat4 S1 = scale(s);
	mat4 T1 = translate(t);
	mat4 ST1 = T1 * S1;          // first scale, then translate
	mat4 TS1 = S1 * T1;          // first translate, then scale
	mat4 ST2 = scale(T1, s);     // first scale, then translate
	mat4 TS2 = translate(S1, t); // first translate, then scale

	CHECK(ST1 == ST2);
	CHECK(TS1 == TS2);
	CHECK(ST1 != TS1); // not commutative

	vec4 p(4, 5, 6, 1); // point
	vec4 d(4, 5, 6, 0); // direction
	CHECK(ST1 * p == vec4( 9, 6, 6, 1));
	CHECK(ST1 * d == vec4( 8, 5, 6, 0));
	CHECK(TS1 * p == vec4(10, 6, 6, 1));
	CHECK(TS1 * d == vec4( 8, 5, 6, 0));
}

TEST_CASE("gl_transform: rotation")
{
	float deg0   = radians(  0.0f);
	float deg90  = radians( 90.0f);
	float deg180 = radians(180.0f);
	float deg270 = radians(270.0f);

	SECTION("x") {
		mat4 R0   = rotateX(deg0);
		mat4 R90  = rotateX(deg90);
		mat4 R180 = rotateX(deg180);
		mat4 R270 = rotateX(deg270);
		CHECK(approxEq(R0,   mat4(vec4(1, 0, 0, 0), vec4(0,  1,  0, 0), vec4(0,  0,  1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R90,  mat4(vec4(1, 0, 0, 0), vec4(0,  0,  1, 0), vec4(0, -1,  0, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R180, mat4(vec4(1, 0, 0, 0), vec4(0, -1,  0, 0), vec4(0,  0, -1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R270, mat4(vec4(1, 0, 0, 0), vec4(0,  0, -1, 0), vec4(0,  1,  0, 0), vec4(0, 0, 0, 1))));

		CHECK(approxEq(rotateX(R90,  deg90 ), R180));
		CHECK(approxEq(rotateX(R90,  deg180), R270));
		CHECK(approxEq(rotateX(R90,  deg270), R0  ));
		CHECK(approxEq(rotateX(R180, deg270), R90 ));

		CHECK(approxEq(inverse(R90 ), R270));
		CHECK(approxEq(inverse(R180), R180));
		CHECK(approxEq(inverse(R90), transpose(R90)));
	}
	SECTION("y") {
		mat4 R0   = rotateY(deg0);
		mat4 R90  = rotateY(deg90);
		mat4 R180 = rotateY(deg180);
		mat4 R270 = rotateY(deg270);
		CHECK(approxEq(R0,   mat4(vec4( 1, 0,  0, 0), vec4(0, 1, 0, 0), vec4( 0, 0,  1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R90,  mat4(vec4( 0, 0, -1, 0), vec4(0, 1, 0, 0), vec4( 1, 0,  0, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R180, mat4(vec4(-1, 0,  0, 0), vec4(0, 1, 0, 0), vec4( 0, 0, -1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R270, mat4(vec4( 0, 0,  1, 0), vec4(0, 1, 0, 0), vec4(-1, 0,  0, 0), vec4(0, 0, 0, 1))));

		CHECK(approxEq(rotateY(R180, deg90 ), R270));
		CHECK(approxEq(rotateY(R180, deg180), R0  ));
		CHECK(approxEq(rotateY(R180, deg270), R90 ));
		CHECK(approxEq(rotateY(R270, deg270), R180));

		CHECK(approxEq(inverse(R90 ), R270));
		CHECK(approxEq(inverse(R180), R180));
		CHECK(approxEq(inverse(R90), transpose(R90)));
	}
	SECTION("z") {
		mat4 R0   = rotateZ(deg0);
		mat4 R90  = rotateZ(deg90);
		mat4 R180 = rotateZ(deg180);
		mat4 R270 = rotateZ(deg270);
		CHECK(approxEq(R0,   mat4(vec4( 1,  0, 0, 0), vec4( 0,  1, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R90,  mat4(vec4( 0,  1, 0, 0), vec4(-1,  0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R180, mat4(vec4(-1,  0, 0, 0), vec4( 0, -1, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1))));
		CHECK(approxEq(R270, mat4(vec4( 0, -1, 0, 0), vec4( 1,  0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1))));

		CHECK(approxEq(rotateZ(R270, deg90 ), R0  ));
		CHECK(approxEq(rotateZ(R270, deg180), R90 ));
		CHECK(approxEq(rotateZ(R270, deg270), R180));
		CHECK(approxEq(rotateZ(R90,  deg270), R0  ));

		CHECK(approxEq(inverse(R90 ), R270));
		CHECK(approxEq(inverse(R180), R180));
		CHECK(approxEq(inverse(R90), transpose(R90)));
	}
	SECTION("arbitrary axis") {
		vec3 axis = normalize(vec3(1, 2, 3));
		mat4 Rx90 = rotateX(deg90);
		mat4 rot1 = rotate(1.23f, axis);
		mat4 rot2 = rotate(Rx90, 1.23f, axis);
		mat4 rot3 = Rx90 * rot1;
		CHECK(approxEq(rot2, rot3));
		vec4 p(-1, 2, 1, 1);
		vec4 q = rot1 * p;
		CHECK(approxEq(q, vec4(-1.05647, 0.231566, 2.19778, 1)));
		CHECK(approxEq(length(p), length(q)));
		CHECK(approxEq(inverse(rot1), rotate(-1.23f, axis)));
		CHECK(approxEq(inverse(rot1), transpose(rot1)));
	}
}

TEST_CASE("gl_transform: ortho")
{
	mat4 O = ortho(0, 640, 0, 480, -1, 1);
	CHECK(approxEq(O, mat4(vec4(0.003125, 0, 0, 0),
			       vec4(0, 0.00416667, 0, 0),
			       vec4(0, 0, -1, 0),
			       vec4(-1, -1, 0, 1))));
}

TEST_CASE("gl_transform: frustum")
{
	mat4 F = frustum(0, 640, 0, 480, -1, 1);
	CHECK(approxEq(F, mat4(vec4(-0.003125, 0, 0, 0),
			       vec4(0, 0.00416667, 0, 0),
			       vec4(1, 1, 0, -1),
			       vec4(0, 0, 1, 0))));
}


#if 0

// The following functions are not part of the actual test. They get compiled,
// but never executed. I used them to (manually) inspect the quality of the
// generated code.

void test_scale(float x, float y, float z, mat4& A)
{
	A = scale(vec3(x, y, z));
}
void test_scale(mat4& A, float x, float y, float z, mat4& B)
{
	B = scale(A, vec3(x, y, z));
}

void test_translate(float x, float y, float z, mat4& A)
{
	A = translate(vec3(x, y, z));
}
void test_translate(mat4& A, float x, float y, float z, mat4& B)
{
	B = translate(A, vec3(x, y, z));
}

void test_rotate(float a, float x, float y, float z, mat4& A)
{
	A = rotate(a, vec3(x, y, z));
}

void test_ortho(float l, float r, float b, float t, float n, float f, mat4& A)
{
	A = ortho(l, r, b, t, n, f);
}

void test_frustum(float l, float r, float b, float t, float n, float f, mat4& A)
{
	A = frustum(l, r, b, t, n, f);
}

#endif
