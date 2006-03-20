// $Id$

#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;

class GLScaleNxScaler: public GLScaler
{
public:
	GLScaleNxScaler();

	virtual void scaleImage(
		PartialColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
		);

private:
	std::auto_ptr<ShaderProgram> scalerProgram;
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
