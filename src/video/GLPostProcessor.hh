// $Id$

#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "GLUtil.hh"
#include <memory>

namespace openmsx {

class Scaler;
class CommandController;
class Display;
class VisibleSurface;

/** Rasterizer using SDL.
  */
class GLPostProcessor : public PostProcessor
{
public:
	GLPostProcessor(
		CommandController& commandController, Display& display,
		VisibleSurface& screen, VideoSource videoSource,
		unsigned maxWidth, unsigned height
		);
	virtual ~GLPostProcessor();

	// Layer interface:
	virtual void paint();

	virtual RawFrame* rotateFrames(
		RawFrame* finishedFrame, FrameSource::FieldType field
		);

private:
	void uploadFrame();
	void paintLines(
		unsigned srcStartY, unsigned srcEndY, unsigned lineWidth, // source
		unsigned dstStartY, unsigned dstEndY // dest
		);

	ColourTexture paintTexture;
	unsigned paintTextureWidth;
	unsigned paintTextureHeight;
	FrameSource* paintFrame;

	std::auto_ptr<ShaderProgram> scalerProgram;
	std::auto_ptr<VertexShader> scalerVertexShader;
	std::auto_ptr<FragmentShader> scalerFragmentShader;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
