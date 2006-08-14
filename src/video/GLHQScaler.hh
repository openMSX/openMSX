// $Id$

#ifndef GLHQSCALER_HH
#define GLHQSCALER_HH

#include "GLScaler.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;
class Texture;

class GLHQScaler : public GLScaler
{
public:
	GLHQScaler();

	virtual void scaleImage(
		ColourTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth);
	virtual void uploadBlock(
		unsigned srcStartY, unsigned srcEndY,
		unsigned lineWidth, FrameSource& paintFrame);

private:
	std::auto_ptr<ShaderProgram> scalerProgram;
	std::auto_ptr<Texture> edgeTexture;
	std::auto_ptr<Texture> offsetTexture[3];
	std::auto_ptr<Texture> weightTexture[3];
};

} // namespace openmsx

#endif
