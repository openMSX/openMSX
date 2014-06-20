#ifndef GLDEFAULTSCALER_HH
#define GLDEFAULTSCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLDefaultScaler : public GLScaler, private noncopyable
{
public:
	GLDefaultScaler();

	virtual void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);
};

} // namespace openmsx

#endif // GLDEFAULTSCALER_HH
