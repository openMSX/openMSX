// $Id$

#ifndef GLHQLITESCALER_HH
#define GLHQLITESCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class ShaderProgram;
class Texture;

class GLHQLiteScaler : public GLScaler, private noncopyable
{
public:
	GLHQLiteScaler();

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
	std::auto_ptr<Texture> weightTexture[3];
};

} // namespace openmsx

#endif
