#include "gl_transform.hh"
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
bool approxEq(float x, float y)
{
	return fabsf(x - y) < 1.0e-5f;
}
bool approxEq(const vec4& x, const vec4&y)
{
	return length2(x - y) < 1.0e-4f;
}
bool approxEq(const mat4& x, const mat4&y)
{
	return norm2_2(x - y) < 1.0e-3f;
}

int main()
{
	{
		// scale
		mat4 S1 = scale(vec3(1,2,3));
		mat4 S2 = scale(S1, vec3(2,3,4)); // first this scale, then S1
		assert(S1 == mat4(vec4(1,0,0,0),vec4(0,2,0,0),vec4(0,0,3,0),vec4(0,0,0,1)));
		assert(S2 == mat4(vec4(2,0,0,0),vec4(0,6,0,0),vec4(0,0,12,0),vec4(0,0,0,1)));

		vec4 p(4,5,6,1); // point
		vec4 d(4,5,6,0); // direction
		assert(S1 * p == vec4(4,10,18,1));
		assert(S1 * d == vec4(4,10,18,0));
		assert(S2 * p == vec4(8,30,72,1));
		assert(S2 * d == vec4(8,30,72,0));

		assert(S1 * S2 == S2 * S1); // scaling is commutative
		assert(approxEq(inverse(S1), scale(vec3(1.0f/1.0f, 1.0f/2.0f, 1.0f/3.0f))));
	}
	{
		// translate
		mat4 T1 = translate(    vec3( 1,2, 3));
		mat4 T2 = translate(T1, vec3(-2,1,-1)); // first this, then T1
		mat4 T3 = translate(    vec3(-1,3, 2));

		assert(T1 == mat4(vec4(1,0,0,0),vec4(0,1,0,0),vec4(0,0,1,0),vec4(1,2,3,1)));
		assert(T2 == mat4(vec4(1,0,0,0),vec4(0,1,0,0),vec4(0,0,1,0),vec4(-1,3,2,1)));
		assert(T2 == T3);

		vec4 p(4,5,6,1); // point
		vec4 d(4,5,6,0); // direction
		assert((T1 * p) == (p + vec4(1,2,3,0)));
		assert((T1 * d) == d);
		assert((T2 * p) == (p + vec4(-1,3,2,0)));
		assert((T2 * d) == d);

		assert(T1 * T2 == T2 * T1); // translation is commutative
		assert(approxEq(inverse(T1), translate(vec3(-1,-2,-3))));
	}
	{
		// scale + translate
		vec3 s(2,1,1);
		vec3 t(1,1,0);
		mat4 S1 = scale(s);
		mat4 T1 = translate(t);
		mat4 ST1 = T1 * S1;          // first scale, then translate
		mat4 TS1 = S1 * T1;          // first translate, then scale
		mat4 ST2 = scale(T1, s);     // first scale, then translate
		mat4 TS2 = translate(S1, t); // first translate, then scale

		assert(ST1 == ST2);
		assert(TS1 == TS2);
		assert(ST1 != TS1); // not commutative

		vec4 p(4,5,6,1); // point
		vec4 d(4,5,6,0); // direction
		assert(ST1 * p == vec4( 9,6,6,1));
		assert(ST1 * d == vec4( 8,5,6,0));
		assert(TS1 * p == vec4(10,6,6,1));
		assert(TS1 * d == vec4( 8,5,6,0));
	}
	{
		// rotation
		float deg0   =  0.0f;
		float deg90  =  M_PI / 2.0f;
		float deg180 =  M_PI;
		float deg270 = -M_PI / 2.0f;

		// x
		mat4 Rx0   = rotateX(deg0);
		mat4 Rx90  = rotateX(deg90);
		mat4 Rx180 = rotateX(deg180);
		mat4 Rx270 = rotateX(deg270);
		assert(approxEq(Rx0,   mat4(vec4(1,0,0,0),vec4(0, 1, 0,0),vec4(0, 0, 1,0),vec4(0,0,0,1))));
		assert(approxEq(Rx90,  mat4(vec4(1,0,0,0),vec4(0, 0, 1,0),vec4(0,-1, 0,0),vec4(0,0,0,1))));
		assert(approxEq(Rx180, mat4(vec4(1,0,0,0),vec4(0,-1, 0,0),vec4(0, 0,-1,0),vec4(0,0,0,1))));
		assert(approxEq(Rx270, mat4(vec4(1,0,0,0),vec4(0, 0,-1,0),vec4(0, 1, 0,0),vec4(0,0,0,1))));

		assert(approxEq(rotateX(Rx90,  deg90 ), Rx180));
		assert(approxEq(rotateX(Rx90,  deg180), Rx270));
		assert(approxEq(rotateX(Rx90,  deg270), Rx0  ));
		assert(approxEq(rotateX(Rx180, deg270), Rx90 ));

		assert(approxEq(inverse(Rx90 ), Rx270));
		assert(approxEq(inverse(Rx180), Rx180));
		assert(approxEq(inverse(Rx90), transpose(Rx90)));

		// y
		mat4 Ry0   = rotateY(deg0);
		mat4 Ry90  = rotateY(deg90);
		mat4 Ry180 = rotateY(deg180);
		mat4 Ry270 = rotateY(deg270);
		assert(approxEq(Ry0,   mat4(vec4( 1,0, 0,0),vec4(0,1,0,0),vec4( 0,0, 1,0),vec4(0,0,0,1))));
		assert(approxEq(Ry90,  mat4(vec4( 0,0,-1,0),vec4(0,1,0,0),vec4( 1,0, 0,0),vec4(0,0,0,1))));
		assert(approxEq(Ry180, mat4(vec4(-1,0, 0,0),vec4(0,1,0,0),vec4( 0,0,-1,0),vec4(0,0,0,1))));
		assert(approxEq(Ry270, mat4(vec4( 0,0, 1,0),vec4(0,1,0,0),vec4(-1,0, 0,0),vec4(0,0,0,1))));

		assert(approxEq(rotateY(Ry180, deg90 ), Ry270));
		assert(approxEq(rotateY(Ry180, deg180), Ry0  ));
		assert(approxEq(rotateY(Ry180, deg270), Ry90 ));
		assert(approxEq(rotateY(Ry270, deg270), Ry180));

		assert(approxEq(inverse(Ry90 ), Ry270));
		assert(approxEq(inverse(Ry180), Ry180));
		assert(approxEq(inverse(Ry90), transpose(Ry90)));

		// z
		mat4 Rz0   = rotateZ(deg0);
		mat4 Rz90  = rotateZ(deg90);
		mat4 Rz180 = rotateZ(deg180);
		mat4 Rz270 = rotateZ(deg270);
		assert(approxEq(Rz0,   mat4(vec4( 1, 0,0,0),vec4( 0, 1,0,0),vec4(0,0,1,0),vec4(0,0,0,1))));
		assert(approxEq(Rz90,  mat4(vec4( 0, 1,0,0),vec4(-1, 0,0,0),vec4(0,0,1,0),vec4(0,0,0,1))));
		assert(approxEq(Rz180, mat4(vec4(-1, 0,0,0),vec4( 0,-1,0,0),vec4(0,0,1,0),vec4(0,0,0,1))));
		assert(approxEq(Rz270, mat4(vec4( 0,-1,0,0),vec4( 1, 0,0,0),vec4(0,0,1,0),vec4(0,0,0,1))));

		assert(approxEq(rotateZ(Rz270, deg90 ), Rz0  ));
		assert(approxEq(rotateZ(Rz270, deg180), Rz90 ));
		assert(approxEq(rotateZ(Rz270, deg270), Rz180));
		assert(approxEq(rotateZ(Rz90,  deg270), Rz0  ));

		assert(approxEq(inverse(Rz90 ), Rz270));
		assert(approxEq(inverse(Rz180), Rz180));
		assert(approxEq(inverse(Rz90), transpose(Rz90)));

		// arbitrary axis
		vec3 axis = normalize(vec3(1,2,3));
		mat4 rot1 = rotate(1.23f, axis);
		mat4 rot2 = rotate(Rx90, 1.23f, axis);
		mat4 rot3 = Rx90 * rot1;
		assert(approxEq(rot2, rot3));
		vec4 p(-1,2,1,1);
		vec4 q = rot1 * p;
		assert(approxEq(q, vec4(-1.05647, 0.231566, 2.19778, 1)));
		assert(approxEq(length(p), length(q)));
		assert(approxEq(inverse(rot1), rotate(-1.23f, axis)));
		assert(approxEq(inverse(rot1), transpose(rot1)));
	}
	{
		// ortho
		mat4 O = ortho(0, 640, 0, 480, -1, 1);
		assert(approxEq(O, mat4(vec4(0.003125, 0, 0, 0),
		                        vec4(0, 0.00416667, 0, 0),
		                        vec4(0, 0, -1, 0),
		                        vec4(-1, -1, 0, 1))));
	}
	{
		// ortho
		mat4 F = frustum(0, 640, 0, 480, -1, 1);
		assert(approxEq(F, mat4(vec4(-0.003125, 0, 0, 0),
		                        vec4(0, 0.00416667, 0, 0),
		                        vec4(1, 1, 0, -1),
		                        vec4(0, 0, 1, 0))));
	}
}


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
