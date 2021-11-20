#ifndef GL_TRANSFORM_HH
#define GL_TRANSFORM_HH

// openGL ES 2.0 removed functions like
//   glRotate(), glTranslate(), glScale(), ...
// This code can be used to replace them.
//
// This code was inspired by the 'glm' library (though written from scratch).
// Compared to glm this code is much simpler but offers much less functionality.
//   http://glm.g-truc.net

#include "gl_mat.hh"
#include "gl_vec.hh"

namespace gl {

// Returns a 4x4 scaling matrix for the given xyz scale factors.
// Comparable to the glScale() function.
[[nodiscard]] constexpr mat4 scale(const vec3& xyz)
{
	return mat4(vec4(xyz, 1.0f));
}

// Multiplies the given matrix by a scaling matrix. Equivalent to (but more
// efficient than) 'A * scale(xyz)'.
[[nodiscard]] constexpr mat4 scale(const mat4& A, const vec3& xyz)
{
	return {A[0] * xyz[0],
	        A[1] * xyz[1],
	        A[2] * xyz[2],
	        A[3]};
}

// Returns a 4x4 translation matrix for the given xyz translation vector.
// Comparable to the gltranslate() function.
[[nodiscard]] constexpr mat4 translate(const vec3& xyz)
{
	mat4 result;
	result[3] = vec4(xyz, 1.0f);
	return result;
}

// Multiplies the given matrix by a translation matrix. Equivalent to (but
// more efficient than) 'A * translate(xyz)'.
[[nodiscard]] constexpr mat4 translate(mat4& A, const vec3& xyz)
{
	return {A[0],
	        A[1],
	        A[2],
	        A[0] * xyz[0] + A[1] * xyz[1] + A[2] * xyz[2] + A[3]};
}

// Returns a 4x4 rotation matrix for rotation of the given 'angle' around the
// 'xyz' axis.
// Comparable to the glRotate() function, but with these differences:
//  - 'angle' is in radians instead of degrees
//  - 'axis' must be a normalized vector
[[nodiscard]] inline mat4 rotate(float angle, const vec3& axis)
{
	float s = sinf(angle);
	float c = cosf(angle);
	vec3 temp = (1.0f - c) * axis;

	return {vec4(axis[0] * temp[0] + c,
	             axis[1] * temp[0] + s * axis[2],
	             axis[2] * temp[0] - s * axis[1],
	             0.0f),
	        vec4(axis[0] * temp[1] - s * axis[2],
	             axis[1] * temp[1] + c,
	             axis[2] * temp[1] + s * axis[0],
	             0.0f),
	        vec4(axis[0] * temp[2] + s * axis[1],
	             axis[1] * temp[2] - s * axis[0],
	             axis[2] * temp[2] + c,
	             0.0f),
	        vec4(0.0f,
	             0.0f,
	             0.0f,
	             1.0f)};
}

// Multiplies the given matrix by a rotation matrix. Equivalent to
// 'A * rotate(angle, axis)'.
[[nodiscard]] inline mat4 rotate(const mat4& A, float angle, const vec3& axis)
{
	// TODO this could be optimized (only a little), though it's rarely used
	return A * rotate(angle, axis);
}

// Returns a 4x4 rotation matrix for rotation around the X-axis. Much more
// efficient than calling the generic rotate() function with a vec3(1,0,0)
// axis.
[[nodiscard]] inline mat4 rotateX(float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {vec4(1.0f, 0.0f, 0.0f, 0.0f),
	        vec4(0.0f,  c  ,  s  , 0.0f),
	        vec4(0.0f, -s  ,  c  , 0.0f),
	        vec4(0.0f, 0.0f, 0.0f, 1.0f)};
}

// Multiplies the given matrix by a X-rotation matrix. Equivalent to (but more
// efficient than) 'A * rotateX(angle)'.
[[nodiscard]] inline mat4 rotateX(const mat4& A, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {A[0],
	        A[1] * c + A[2] * s,
	        A[2] * c - A[1] * s,
	        A[3]};
}

// Returns a 4x4 rotation matrix for rotation around the Y-axis. Much more
// efficient than calling the generic rotate() function with a vec3(0,1,0)
// axis.
[[nodiscard]] inline mat4 rotateY(float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {vec4( c  , 0.0f, -s  , 0.0f),
	        vec4(0.0f, 1.0f, 0.0f, 0.0f),
	        vec4( s  , 0.0f,  c  , 0.0f),
	        vec4(0.0f, 0.0f, 0.0f, 1.0f)};
}

// Multiplies the given matrix by a Y-rotation matrix. Equivalent to (but more
// efficient than) 'A * rotateY(angle)'.
[[nodiscard]] inline mat4 rotateY(const mat4& A, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {A[0] * c - A[2] * s,
	        A[1],
	        A[2] * c + A[0] * s,
	        A[3]};
}

// Returns a 4x4 rotation matrix for rotation around the Z-axis. Much more
// efficient than calling the generic rotate() function with a vec3(0,0,1)
// axis.
[[nodiscard]] inline mat4 rotateZ(float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {vec4( c  ,  s  , 0.0f, 0.0f),
	        vec4(-s  ,  c  , 0.0f, 0.0f),
	        vec4(0.0f, 0.0f, 1.0f, 0.0f),
	        vec4(0.0f, 0.0f, 0.0f, 1.0f)};
}

// Multiplies the given matrix by a Z-rotation matrix. Equivalent to (but more
// efficient than) 'A * rotateZ(angle)'.
[[nodiscard]] inline mat4 rotateZ(const mat4& A, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return {A[0] * c + A[1] * s,
	        A[1] * c - A[0] * s,
	        A[2],
	        A[3]};
}

// Note: Don't use "near" or "far" as names since those are macros on Windows.

// Returns a 4x4 orthographic projection matrix. Comparable to
// the glOrtho() function.
[[nodiscard]] constexpr mat4 ortho(
	float left,    float right,
	float bottom,  float top,
	float nearVal, float farVal)
{
	return {vec4(-2.0f / (left - right),  0.0f, 0.0f, 0.0f),
	        vec4( 0.0f, -2.0f / (bottom - top), 0.0f, 0.0f),
	        vec4( 0.0f,  0.0f,  2.0f / (nearVal - farVal),  0.0f),
	        vec4((left    + right ) / (left    - right ),
	             (bottom  + top   ) / (bottom  - top   ),
	             (nearVal + farVal) / (nearVal - farVal),
	             1.0f)};
}

// Returns a 4x4 frustum projection matrix. Comparable to
// the glFrustum() function.
[[nodiscard]] constexpr mat4 frustum(
	float left,    float right,
	float bottom,  float top,
	float nearVal, float farVal)
{
	return {vec4((2.0f * nearVal) / (right - left), 0.0f, 0.0f, 0.0f),
	        vec4(0.0f, (2.0f * nearVal) / (top - bottom), 0.0f, 0.0f),
	        vec4((right   + left  ) / (right   - left  ),
	             (top     + bottom) / (top     - bottom),
	             (nearVal + farVal) / (nearVal - farVal),
	             -1.0f),
	        vec4(0.0f,
	             0.0f,
	             (2.0f * farVal * nearVal) / (nearVal - farVal),
	             0.0f)};
}

} // namespace gl

#endif
