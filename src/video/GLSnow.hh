// $Id$

#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "Layer.hh"
#include "GLUtil.hh"

namespace openmsx {

class Display;

/** Snow effect for background layer.
  */
class GLSnow: public Layer
{
public:
	GLSnow(Display& display);
	virtual ~GLSnow();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	GLuint noiseTextureId;
};

} // namespace openmsx

#endif
