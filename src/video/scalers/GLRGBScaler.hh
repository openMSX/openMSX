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
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

	[[nodiscard]] gl::ivec2 getOutputScaleSize(gl::ivec2 /*dstScreenSize*/) const override {
		return 3 * gl::ivec2{320, 240};
	}

private:
	RenderSettings& renderSettings;
	GLScaler& fallback;
	std::array<int, 2> unifCnsts;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
