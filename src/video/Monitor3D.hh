#ifndef MONITOR3D_HH
#define MONITOR3D_HH

// Geometry of the "3d" display deform: the picture is drawn on a curved
// surface, seen from slightly above. The surface spans x and y in [-1, 1].
// Used by PostProcessor to draw it, and by the Gunstick to map the mouse
// back to the picture.

#include "gl_mat.hh"
#include "gl_transform.hh"
#include "gl_vec.hh"

namespace openmsx::monitor3d {

inline constexpr float CURVATURE = 1.0f / 12.0f;

/** Height of the surface at the given point. */
[[nodiscard]] constexpr float surfaceZ(gl::vec2 xy)
{
	return -CURVATURE * length2(xy);
}

/** Texture coordinate of the given surface point.
  * @param width The horizontal_stretch setting: the picture shows the
  *              middle 'width' of the 320 texture columns.
  */
[[nodiscard]] constexpr gl::vec2 texCoord(gl::vec2 xy, float width)
{
	float s = width * (1.0f / 320.0f);
	float b = (320.0f - width) * (1.0f / (2.0f * 320.0f));
	return {(xy.x + 1.0f) * 0.5f * s + b,
	        (xy.y + 1.0f) * 0.5f};
}

/** Rotation of the surface, also needed to transform the normals. */
[[nodiscard]] inline gl::mat4 rotation()
{
	return gl::rotateX(gl::radians(-10.0f));
}

/** Surface coordinates -> clip coordinates. */
[[nodiscard]] inline gl::mat4 mvpMatrix()
{
	gl::mat4 proj = gl::frustum(-1, 1, -1, 1, 1, 10);
	gl::mat4 tran = gl::translate(gl::vec3(0.0f, 0.4f, -2.0f));
	gl::mat4 scal = gl::scale(gl::vec3(2.2f, 2.2f, 2.2f));
	return proj * tran * rotation() * scal;
}

} // namespace openmsx::monitor3d

#endif
