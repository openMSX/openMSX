// $Id$

#ifndef GLHQSCALER_HH
#define GLHQSCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;
class Texture;
template <typename T> class PixelBuffer;

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
	std::unique_ptr<ShaderProgram> scalerProgram[2];
	std::unique_ptr<Texture> edgeTexture;
	std::unique_ptr<Texture> offsetTexture[3];
	std::unique_ptr<Texture> weightTexture[3];
	std::unique_ptr<PixelBuffer<unsigned short> > edgeBuffer;
};

} // namespace openmsx

#endif
