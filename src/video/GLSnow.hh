// $Id$

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
	GLSnow(Display& display, unsigned width, unsigned height);
	virtual ~GLSnow();

	// Layer interface:
	virtual void paint(OutputSurface& output);
	virtual const std::string& getName();

private:
	Display& display;
	const unsigned width;
	const unsigned height;
	GLuint noiseTextureId;
};

} // namespace openmsx

#endif
