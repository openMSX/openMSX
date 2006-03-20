// $Id$

#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "GLUtil.hh"
#include <memory>

namespace openmsx {

class GLScaler;
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

protected:
	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	void uploadFrame();

	void preCalcNoise(double factor);
	void drawNoise();

	/** The currently active scaler.
	  */
	std::auto_ptr<GLScaler> currScaler;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm;

	PartialColourTexture paintTexture;
	FrameSource* paintFrame;

	// Noise effect:
	LuminanceTexture noiseTextureA;
	LuminanceTexture noiseTextureB;
	unsigned noiseSeq;
	double noiseX;
	double noiseY;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
