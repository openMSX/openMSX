#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "GLUtil.hh"
#include "Layer.hh"

#include <array>

namespace openmsx {

/** Snow effect for background layer.
  */
class GLSnow final : public Layer
{
public:
	GLSnow();

	// Layer interface:
	void paint(const OutputDimensions& output) override;

private:
	std::array<gl::BufferObject, 2> vbo;
	gl::Texture noiseTexture;
};

} // namespace openmsx

#endif
