#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

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
	ShaderProgram scalerProgram[2];
	GLint texSizeLoc[2];
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
