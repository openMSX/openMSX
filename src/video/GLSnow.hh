#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "Layer.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class Display;

/** Snow effect for background layer.
  */
class GLSnow : public Layer, private noncopyable
{
public:
	GLSnow(Display& display);

	// Layer interface:
	virtual void paint(OutputSurface& output);

private:
	Display& display;
	gl::Texture noiseTexture;
	gl::ShaderProgram texProg;
};

} // namespace openmsx

#endif
