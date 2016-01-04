#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "Layer.hh"
#include "GLUtil.hh"

namespace openmsx {

class Display;

/** Snow effect for background layer.
  */
class GLSnow final : public Layer
{
public:
	GLSnow(Display& display);

	// Layer interface:
	void paint(OutputSurface& output) override;

private:
	Display& display;
	gl::Texture noiseTexture;
};

} // namespace openmsx

#endif
