#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLScaleNxScaler : public GLScaler, private noncopyable
{
public:
	explicit GLScaleNxScaler(GLScaler& fallback);

	virtual void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	GLScaler& fallback;
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
