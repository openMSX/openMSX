#ifndef GLRGBSCALER_HH
#define GLRGBSCALER_HH

#include "GLScaler.hh"
#include <array>

namespace openmsx {

class RenderSettings;

class GLRGBScaler final : public GLScaler
{
public:
	GLRGBScaler(RenderSettings& renderSettings, GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;

private:
	RenderSettings& renderSettings;
	GLScaler& fallback;
	std::array<int, 2> unifCnsts;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
