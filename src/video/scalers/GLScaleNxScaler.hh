#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"

namespace openmsx {

class GLScaleNxScaler final : public GLScaler
{
public:
	explicit GLScaleNxScaler(GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

	[[nodiscard]] gl::ivec2 getOutputScaleSize(gl::ivec2 dstScreenSize) const override {
		int factor = (dstScreenSize.y + 240 - 1) / 240;
		return gl::ivec2{320, 240} * factor;
	}

private:
	GLScaler& fallback;
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
