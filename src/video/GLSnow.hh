// $Id$

#ifndef GLSNOW_HH
#define GLSNOW_HH

#include "Layer.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

/** Snow effect for background layer.
  */
class GLSnow : public Layer, private noncopyable
{
public:
	GLSnow(unsigned width, unsigned height);
	virtual ~GLSnow();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	const unsigned width;
	const unsigned height;
	GLuint noiseTextureId;
};

} // namespace openmsx

#endif
