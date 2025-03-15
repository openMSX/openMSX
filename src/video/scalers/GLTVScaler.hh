#ifndef GLTVSCALER_HH
#define GLTVSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include <array>

namespace openmsx {

class RenderSettings;

class GLTVScaler final : public GLScaler
{
public:
	explicit GLTVScaler(RenderSettings& renderSettings);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

private:
	RenderSettings& renderSettings;
	std::array<GLint, 2> unifMinScanline;
	std::array<GLint, 2> unifSizeVariance;
};

} // namespace openmsx

#endif // GLTVSCALER_HH
