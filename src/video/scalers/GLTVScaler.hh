#ifndef GLTVSCALER_HH
#define GLTVSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"

namespace openmsx {

class RenderSettings;

class GLTVScaler final : public GLScaler
{
public:
	explicit GLTVScaler(RenderSettings& renderSettings);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;

private:
	RenderSettings& renderSettings;
	GLint unifMinScanline [2];
	GLint unifSizeVariance[2];
};

} // namespace openmsx

#endif // GLTVSCALER_HH
