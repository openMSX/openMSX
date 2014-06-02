#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class RenderSettings;

class GLSimpleScaler: public GLScaler, private noncopyable
{
public:
	GLSimpleScaler(RenderSettings& renderSettings, GLScaler& fallback);

	virtual void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	RenderSettings& renderSettings;
	GLScaler& fallback;
	struct Data {
		gl::ShaderProgram scalerProgram;
		int texSizeLoc;
		int texStepXLoc;
		int cnstLoc;
	} data[2];
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
