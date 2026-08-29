#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "GLUtil.hh"

#include <array>

namespace openmsx {

/** Snow effect for background layer.
  */
class GLSnow
{
public:
	GLSnow();
	void paint();

private:
	std::array<gl::BufferObject, 2> vbo;
	gl::Texture noiseTexture;
};

} // namespace openmsx

#endif
