#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"
#include <array>

namespace openmsx {

class RenderSettings;

class GLSimpleScaler final : public GLScaler
{
public:
	GLSimpleScaler(RenderSettings& renderSettings, GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

	[[nodiscard]] gl::ivec2 getOutputScaleSize(gl::ivec2 dstScreenSize) const override {
		return dstScreenSize; // can do arbitrary scaling
	}

private:
	RenderSettings& renderSettings;
	GLScaler& fallback;
	std::array<int, 2> unifTexStepX;
	std::array<int, 2> unifCnst;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
