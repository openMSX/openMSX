#ifndef GLHQSCALER_HH
#define GLHQSCALER_HH

#include "GLScaler.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class GLHQScaler final : public GLScaler
{
public:
	explicit GLHQScaler(GLScaler& fallback);

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
	gl::Texture edgeTexture320, edgeTexture640;
	std::array<gl::Texture, 3> offsetTexture;
	std::array<gl::Texture, 3> weightTexture;
	gl::PixelBuffer<uint16_t> edgeBuffer320, edgeBuffer640;
};

} // namespace openmsx

#endif
