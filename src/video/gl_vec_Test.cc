#include "gl_vec.hh"
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

// Test approximations.
template<typename T>
bool approxEq(T x, T y)
{
	return fabs(x - y) < 1.0e-5f;
}
template<int N, typename T>
bool approxEq(const vecN<N, T>& x, const vecN<N, T>&y)
{
	return length2(x - y) < 1.0e-4f;
}

int main()
{
	{
		// rsqrt
		assert(rsqrt(16.0f) == 0.25f);
		assert(rsqrt(16.0 ) == 0.25 );
	}
	{
		// radians, degrees
		assert(approxEq(radians(  0.0), 0.0     ));
		assert(approxEq(radians( 90.0), M_PI / 2));
		assert(approxEq(radians(180.0), M_PI    ));
		assert(approxEq(degrees(0.0     ),   0.0));
		assert(approxEq(degrees(M_PI / 2),  90.0));
		assert(approxEq(degrees(M_PI    ), 180.0));
	}
	// It's useful to test both integer and float variants because the
	// former are implemented in plain C++ and (only) the latter have SSE
	// optimizations.
	{
		// default constructor
		vec2 v2;
		assert(v2[0] == 0);
		assert(v2[1] == 0);
		ivec2 i2;
		assert(i2[0] == 0);
		assert(i2[1] == 0);

		vec3 v3;
		assert(v3[0] == 0);
		assert(v3[1] == 0);
		assert(v3[2] == 0);
		ivec3 i3;
		assert(i3[0] == 0);
		assert(i3[1] == 0);
		assert(i3[2] == 0);

		vec4 v4;
		assert(v4[0] == 0);
		assert(v4[1] == 0);
		assert(v4[2] == 0);
		assert(v4[3] == 0);
		ivec4 i4;
		assert(i4[0] == 0);
		assert(i4[1] == 0);
		assert(i4[2] == 0);
		assert(i4[3] == 0);
	}
	{
		// broadcast constructor
		vec2 v2(2);
		assert(v2[0] == 2);
		assert(v2[1] == 2);
		ivec2 i2(2);
		assert(i2[0] == 2);
		assert(i2[1] == 2);

		vec3 v3(3);
		assert(v3[0] == 3);
		assert(v3[1] == 3);
		assert(v3[2] == 3);
		ivec3 i3(3);
		assert(i3[0] == 3);
		assert(i3[1] == 3);
		assert(i3[2] == 3);

		vec4 v4(4);
		assert(v4[0] == 4);
		assert(v4[1] == 4);
		assert(v4[2] == 4);
		assert(v4[3] == 4);
		ivec4 i4(4);
		assert(i4[0] == 4);
		assert(i4[1] == 4);
		assert(i4[2] == 4);
		assert(i4[3] == 4);
	}
	{
		// per-element constructor
		vec2 v2(2,3);
		assert(v2[0] == 2);
		assert(v2[1] == 3);
		ivec2 i2(2,3);
		assert(i2[0] == 2);
		assert(i2[1] == 3);

		vec3 v3(3,4,5);
		assert(v3[0] == 3);
		assert(v3[1] == 4);
		assert(v3[2] == 5);
		ivec3 i3(3,4,5);
		assert(i3[0] == 3);
		assert(i3[1] == 4);
		assert(i3[2] == 5);

		vec4 v4(4,5,6,7);
		assert(v4[0] == 4);
		assert(v4[1] == 5);
		assert(v4[2] == 6);
		assert(v4[3] == 7);
		ivec4 i4(4,5,6,7);
		assert(i4[0] == 4);
		assert(i4[1] == 5);
		assert(i4[2] == 6);
		assert(i4[3] == 7);
	}
	{
		// construct from smaller vectors/scalar
		vec3 v3(3, vec2(4,5));
		assert(v3[0] == 3);
		assert(v3[1] == 4);
		assert(v3[2] == 5);
		ivec3 i3(ivec2(4,5), 3);
		assert(i3[0] == 4);
		assert(i3[1] == 5);
		assert(i3[2] == 3);

		vec4 v4(4, vec3(5,6,7));
		assert(v4[0] == 4);
		assert(v4[1] == 5);
		assert(v4[2] == 6);
		assert(v4[3] == 7);
		ivec4 i4(ivec3(5,6,7), 4);
		assert(i4[0] == 5);
		assert(i4[1] == 6);
		assert(i4[2] == 7);
		assert(i4[3] == 4);

		vec4 w4(vec2(5,6), vec2(7,8));
		assert(w4[0] == 5);
		assert(w4[1] == 6);
		assert(w4[2] == 7);
		assert(w4[3] == 8);
		ivec4 j4(ivec2(5,6), ivec2(7,8));
		assert(j4[0] == 5);
		assert(j4[1] == 6);
		assert(j4[2] == 7);
		assert(j4[3] == 8);
	}
	{
		// modify elements
		vec2 v2;
		v2[0] = 5;
		assert(v2[0] == 5);
		assert(v2[1] == 0);
		v2[1] = 7;
		assert(v2[0] == 5);
		assert(v2[1] == 7);
		ivec2 i2;
		i2[0] = 5;
		assert(i2[0] == 5);
		assert(i2[1] == 0);
		i2[1] = 7;
		assert(i2[0] == 5);
		assert(i2[1] == 7);

		vec3 v3(1);
		v3[1] = 2;
		assert(v3[0] == 1);
		assert(v3[1] == 2);
		assert(v3[2] == 1);
		ivec3 i3(1);
		i3[1] = 2;
		assert(i3[0] == 1);
		assert(i3[1] == 2);
		assert(i3[2] == 1);

		vec4 v4(1,2,3,4);
		v4[0] = 9; v4[2] = 8;
		assert(v4[0] == 9);
		assert(v4[1] == 2);
		assert(v4[2] == 8);
		assert(v4[3] == 4);
		ivec4 i4(1,2,3,4);
		i4[0] = 9; i4[2] = 8;
		assert(i4[0] == 9);
		assert(i4[1] == 2);
		assert(i4[2] == 8);
		assert(i4[3] == 4);
	}
	{
		// (in)equality
		assert( vec2(1,2) ==  vec2(1,2));
		assert( vec2(1,2) !=  vec2(1,4));
		assert( vec2(1,2) !=  vec2(3,2));
		assert( vec2(1,2) !=  vec2(3,4));
		assert(ivec2(1,2) == ivec2(1,2));
		assert(ivec2(1,2) != ivec2(1,4));
		assert(ivec2(1,2) != ivec2(3,2));
		assert(ivec2(1,2) != ivec2(3,4));

		assert( vec3(1,2,3) ==  vec3(1,2,3));
		assert( vec3(1,2,3) !=  vec3(1,2,4));
		assert(ivec3(1,2,3) == ivec3(1,2,3));
		assert(ivec3(1,2,3) != ivec3(1,2,4));

		assert( vec4(1,2,3,4) ==  vec4(1,2,3,4));
		assert( vec4(1,2,3,4) !=  vec4(1,2,4,4));
		assert(ivec4(1,2,3,4) == ivec4(1,2,3,4));
		assert(ivec4(1,2,3,4) != ivec4(1,2,4,4));
	}
	{
		// copy constructor, assignment
		vec2 v2(2,3);
		vec2 w2(v2);
		assert(v2 == w2);
		v2[0] = 9; w2 = v2;
		assert(v2 == w2);

		ivec2 i2(2,3);
		ivec2 j2(i2);
		assert(i2 == j2);
		i2[1] = 9; j2 = i2;
		assert(i2 == j2);

		vec3 v3(3,4,5);
		vec3 w3(v3);
		assert(v3 == w3);
		v3[2] = 8; w3 = v3;
		assert(v3 == w3);

		ivec3 i3(3,4,5);
		ivec3 j3(i3);
		assert(i3 == j3);
		i3[1] = 8; j3 = i3;
		assert(i3 == j3);

		vec4 v4(4,5,6,7);
		vec4 w4(v4);
		assert(v4 == w4);
		v3[3] = 0; w4 = v4;
		assert(v4 == w4);

		ivec4 i4(4,5,6,7);
		ivec4 j4(i4);
		assert(i4 == j4);
		i3[0] = 1; j4 = i4;
		assert(i4 == j4);
	}
	{
		// construct from larger vector
		vec4 v4(1,2,3,4);
		vec3 v3(7,8,9);
		assert(vec3(v4) == vec3(1,2,3));
		assert(vec2(v4) == vec2(1,2));
		assert(vec2(v3) == vec2(7,8));

		ivec4 i4(1,2,3,4);
		ivec3 i3(7,8,9);
		assert(ivec3(i4) == ivec3(1,2,3));
		assert(ivec2(i4) == ivec2(1,2));
		assert(ivec2(i3) == ivec2(7,8));
	}
	// From here on I'll skip tests on vec2 and ivec2.
	// Vectors of dimension 2 and 3 are handled by the same C++ code, so
	// if there's a bug it should show up in both types.
	{
		// vector add
		 vec3 v3(1,2,3);  vec4 v4(1,2,3,4);
		ivec3 i3(1,2,3); ivec4 i4(1,2,3,4);
		assert((v3 +  vec3(4,5,6))   ==  vec3(5,7,9));
		assert((i3 + ivec3(4,5,6))   == ivec3(5,7,9));
		assert((v4 +  vec4(4,5,6,7)) ==  vec4(5,7,9,11));
		assert((i4 + ivec4(4,5,6,7)) == ivec4(5,7,9,11));

		v3 +=  vec3(2,3,4);
		i3 += ivec3(2,3,4);
		v4 +=  vec4(2,3,4,5);
		i4 += ivec4(2,3,4,5);
		assert(v3 ==  vec3(3,5,7));
		assert(i3 == ivec3(3,5,7));
		assert(v4 ==  vec4(3,5,7,9));
		assert(i4 == ivec4(3,5,7,9));
	}
	{
		// vector subtract
		 vec3 v3(1,2,3);  vec4 v4(1,2,3,4);
		ivec3 i3(1,2,3); ivec4 i4(1,2,3,4);
		assert((v3 -  vec3(4,3,2))   ==  vec3(-3,-1,1));
		assert((i3 - ivec3(4,3,2))   == ivec3(-3,-1,1));
		assert((v4 -  vec4(4,3,2,1)) ==  vec4(-3,-1,1,3));
		assert((i4 - ivec4(4,3,2,1)) == ivec4(-3,-1,1,3));

		v3 -=  vec3(2,4,6);
		i3 -= ivec3(2,4,6);
		v4 -=  vec4(2,4,6,8);
		i4 -= ivec4(2,4,6,8);
		assert(v3 ==  vec3(-1,-2,-3));
		assert(i3 == ivec3(-1,-2,-3));
		assert(v4 ==  vec4(-1,-2,-3,-4));
		assert(i4 == ivec4(-1,-2,-3,-4));
	}
	{
		// vector negate
		assert((- vec3(4,-3,2))    ==  vec3(-4,3,-2));
		assert((-ivec3(4,-3,2))    == ivec3(-4,3,-2));
		assert((- vec4(4,-3,2,-1)) ==  vec4(-4,3,-2,1));
		assert((-ivec4(4,-3,2,-1)) == ivec4(-4,3,-2,1));
	}
	{
		// component-wise vector multiplication
		 vec3 v3(0,2,-3);  vec4 v4(0,2,-3,4);
		ivec3 i3(0,2,-3); ivec4 i4(0,2,-3,4);
		// scalar * vector
		assert((2.0f * v3) ==  vec3(0,4,-6));
		assert((2    * i3) == ivec3(0,4,-6));
		assert((2.0f * v4) ==  vec4(0,4,-6,8));
		assert((2    * i4) == ivec4(0,4,-6,8));
		// vector * scalar
		assert((v3 * 2.0f) ==  vec3(0,4,-6));
		assert((i3 * 2   ) == ivec3(0,4,-6));
		assert((v4 * 2.0f) ==  vec4(0,4,-6,8));
		assert((i4 * 2   ) == ivec4(0,4,-6,8));
		// vector * vector
		 vec3 w3(-1,2,-3);  vec4 w4(-1,2,-3,-4);
		ivec3 j3(-1,2,-3); ivec4 j4(-1,2,-3,-4);
		assert((v3 * w3) ==  vec3(0,4,9));
		assert((i3 * j3) == ivec3(0,4,9));
		assert((v4 * w4) ==  vec4(0,4,9,-16));
		assert((i4 * j4) == ivec4(0,4,9,-16));
		// *= scalar
		v3 *= 2.0f; assert(v3 ==  vec3(0,4,-6));
		i3 *= 2;    assert(i3 == ivec3(0,4,-6));
		v4 *= 2.0f; assert(v4 ==  vec4(0,4,-6,8));
		i4 *= 2;    assert(i4 == ivec4(0,4,-6,8));
		// *= vector
		v3 *= w3; assert(v3 ==  vec3(0,8,18));
		i3 *= j3; assert(i3 == ivec3(0,8,18));
		v4 *= w4; assert(v4 ==  vec4(0,8,18,-32));
		i4 *= j4; assert(i4 == ivec4(0,8,18,-32));
	}
	{
		// reciprocal, only floating point
		assert(approxEq(recip(vec3(4,2,0.5)),   vec3(0.25,0.5,2)));
		assert(approxEq(recip(vec4(4,2,0.5,1)), vec4(0.25,0.5,2,1)));
	}
	{
		// component-wise division, only floating point
		vec3 v3( 0, 1,-4);
		vec3 w3(-1,-2, 2);
		assert((v3 / 2.0f) == vec3(0,0.5f,-2));
		assert((6.0f / w3) == vec3(-6,-3,3));
		assert((v3 / w3) == vec3(0,-0.5f,-2));

		vec4 v4( 2, 1,-4,-3);
		vec4 w4(-1, 2, 2,-6);
		assert(approxEq((v4 / -2.0f), vec4(-1,-0.5f,2,1.5f)));
		assert(approxEq((6.0f / w4), vec4(-6,3,3,-1)));
		assert(approxEq((v4 / w4), vec4(-2,0.5f,-2,0.5f)));
	}
	{
		// component-wise min/max
		assert(min( vec3(2,3,4),  vec3(1,-5,7)) ==  vec3(1,-5,4));
		assert(min(ivec3(2,3,4), ivec3(1,-5,7)) == ivec3(1,-5,4));
		assert(min( vec4(1,-2,5,-7),  vec4(0,2,-4,-3)) ==  vec4(0,-2,-4,-7));
		assert(min(ivec4(1,-2,5,-7), ivec4(0,2,-4,-3)) == ivec4(0,-2,-4,-7));

		assert(max( vec3(2,3,4),  vec3(1,-5,7)) ==  vec3(2,3,7));
		assert(max(ivec3(2,3,4), ivec3(1,-5,7)) == ivec3(2,3,7));
		assert(max( vec4(1,-2,5,-7),  vec4(0,2,-4,-3)) ==  vec4(1,2,5,-3));
		assert(max(ivec4(1,-2,5,-7), ivec4(0,2,-4,-3)) == ivec4(1,2,5,-3));
	}
	{
		// clamp
		assert(clamp( vec3(2,3,4),  vec3(0,4,-4),  vec3(4,7,0)) ==  vec3(2,4,0));
		assert(clamp(ivec3(2,3,4), ivec3(0,4,-4), ivec3(4,7,0)) == ivec3(2,4,0));
		assert(clamp( vec4(4,2,7,1),  vec4(0,3,2,1),  vec4(4,6,8,3)) ==  vec4(4,3,7,1));
		assert(clamp(ivec4(4,2,7,1), ivec4(0,3,2,1), ivec4(4,6,8,3)) == ivec4(4,3,7,1));

		assert(clamp( vec3(2,3,4), 1.0f, 3.0f) ==  vec3(2,3,3));
		assert(clamp(ivec3(2,3,4), 1,    3   ) == ivec3(2,3,3));
		assert(clamp( vec4(4,2,7,1), 2.0f, 6.0f) ==  vec4(4,2,6,2));
		assert(clamp(ivec4(4,2,7,1), 2,    6   ) == ivec4(4,2,6,2));
	}
	{
		// sum of vector components
		assert(sum( vec3(4,-3,2))    == 3.0f);
		assert(sum(ivec3(4,-3,2))    == 3   );
		assert(sum( vec4(4,-3,2,-1)) == 2.0f);
		assert(sum(ivec4(4,-3,2,-1)) == 2   );

		assert(sum_broadcast(vec3(4,-3,2))    == vec3(3.0f));
		assert(sum_broadcast(vec4(4,-3,2,-1)) == vec4(2.0f));
	}
	{
		// dot product
		assert(dot( vec3(4,-3,2),     vec3(-1,3,2)   ) == -9.0f);
		assert(dot(ivec3(4,-3,2),    ivec3(-1,3,2)   ) == -9   );
		assert(dot( vec4(4,-3,2,-1),  vec4(-1,3,2,-2)) == -7.0f);
		assert(dot(ivec4(4,-3,2,-1), ivec4(-1,3,2,-2)) == -7   );

		assert(dot_broadcast(vec3(4,-3,2),    vec3(-1,3,2)   ) == vec3(-9.0f));
		assert(dot_broadcast(vec4(4,-3,2,-1), vec4(-1,3,2,-2)) == vec4(-7.0f));
	}
	{
		// cross product (only for vectors of length 3)
		assert(cross( vec3(4,-3,2),  vec3(-1,3,2)) ==  vec3(-12,-10,9));
		assert(cross(ivec3(4,-3,2), ivec3(-1,3,2)) == ivec3(-12,-10,9));
	}
	{
		// vector length squared (2-norm squared)
		assert(length2( vec3(4,-3,2   )) == 29.0f);
		assert(length2(ivec3(4,-3,2   )) == 29   );
		assert(length2( vec4(4,-3,2,-1)) == 30.0f);
		assert(length2(ivec4(4,-3,2,-1)) == 30   );
	}
	{
		// vector length (2-norm), only makes sense for floating point
		assert(length(vec3(4,-3,2   )) == sqrtf(29.0f));
		assert(length(vec4(4,-3,2,-1)) == sqrtf(30.0f));
	}
	{
		// vector normalization, only floating point
		assert(normalize(vec3(0,4,-3  )) == vec3(0.0f,0.8f,-0.6f));
		assert(normalize(vec4(-4,0,0,3)) == vec4(-0.8f,0.0f,0.0f,0.6f));
	}
	{
		// round
		assert(round(vec3(1.1f,2.5f,-3.8f))       == ivec3(1,3,-4));
		assert(round(vec4(-1.1f,2.5f,3.8f,-4.5f)) == ivec4(-1,3,4,-5));
		// round integers, nop
		assert(round(ivec4(1,-2,3,-4)) == ivec4(1,-2,3,-4));
	}
	{
		// trunc
		assert(trunc(vec3(1.1f,2.5f,-3.8f))       == ivec3(1,2,-3));
		assert(trunc(vec4(-1.1f,2.5f,3.8f,-4.5f)) == ivec4(-1,2,3,-4));
		// trunc integers, nop
		assert(trunc(ivec4(1,-2,3,-4)) == ivec4(1,-2,3,-4));
	}
}


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
