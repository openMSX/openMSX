// $Id$

#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;

class GLScaleNxScaler : public GLScaler, private noncopyable
{
public:
	GLScaleNxScaler();

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	std::auto_ptr<ShaderProgram> scalerProgram;
	GLint texSizeLoc;
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
