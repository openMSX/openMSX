#ifndef GLDEFAULTSCALER_HH
#define GLDEFAULTSCALER_HH

#include "GLScaler.hh"

namespace openmsx {

class GLDefaultScaler final : public GLScaler
{
public:
	GLDefaultScaler();

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

	[[nodiscard]] gl::ivec2 getOutputScaleSize(gl::ivec2 dstScreenSize) const override {
		return dstScreenSize; // can do arbitrary scaling
	}
};

} // namespace openmsx

#endif // GLDEFAULTSCALER_HH
