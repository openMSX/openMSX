#ifndef GLHQSCALER_HH
#define GLHQSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"
#include <cstdint>

namespace openmsx {

class GLHQScaler : public GLScaler, private noncopyable
{
public:
	GLHQScaler();

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);
	virtual void uploadBlock(
		unsigned srcStartY, unsigned srcEndY,
		unsigned lineWidth, FrameSource& paintFrame);

private:
	ShaderProgram scalerProgram[2];
	Texture edgeTexture;
	Texture offsetTexture[3];
	Texture weightTexture[3];
	PixelBuffer<uint16_t> edgeBuffer;
};

} // namespace openmsx

#endif
