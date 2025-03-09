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
};

} // namespace openmsx

#endif // GLDEFAULTSCALER_HH
