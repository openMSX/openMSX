#ifndef GLHQLITESCALER_HH
#define GLHQLITESCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class GLHQLiteScaler final : public GLScaler
{
public:
	explicit GLHQLiteScaler(GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;
	void uploadBlock(
		unsigned srcStartY, unsigned srcEndY,
		unsigned lineWidth, FrameSource& paintFrame) override;

private:
	GLScaler& fallback;
	gl::Texture edgeTexture;
	std::array<gl::Texture, 3> offsetTexture;
	gl::PixelBuffer<uint16_t> edgeBuffer;
};

} // namespace openmsx

#endif
