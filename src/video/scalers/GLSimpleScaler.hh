#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"

namespace openmsx {

class RenderSettings;

class GLSimpleScaler final : public GLScaler
{
public:
	GLSimpleScaler(RenderSettings& renderSettings, GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;

private:
	RenderSettings& renderSettings;
	GLScaler& fallback;
	int unifTexStepX[2];
	int unifCnst[2];
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
