// $Id$

#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "GLUtil.hh"
#include <map>
#include <vector>
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
	void createRegions();
	void uploadFrame();
	void uploadBlock(unsigned srcStartY, unsigned srcEndY,
	                 unsigned lineWidth);

	void preCalcNoise(double factor);
	void drawNoise();

	/** The currently active scaler.
	  */
	std::auto_ptr<GLScaler> currScaler;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm;

	typedef std::map<unsigned, ColourTexture*> Textures;
	Textures textures;
	unsigned height;

	FrameSource* paintFrame;

	// Noise effect:
	LuminanceTexture noiseTextureA;
	LuminanceTexture noiseTextureB;
	unsigned noiseSeq;
	double noiseX;
	double noiseY;

	struct Region {
		Region(unsigned srcStartY_, unsigned srcEndY_,
		       unsigned dstStartY_, unsigned dstEndY_,
		       unsigned lineWidth_)
			: srcStartY(srcStartY_)
			, srcEndY(srcEndY_)
			, dstStartY(dstStartY_)
			, dstEndY(dstEndY_)
			, lineWidth(lineWidth_) {}
		unsigned srcStartY;
		unsigned srcEndY;
		unsigned dstStartY;
		unsigned dstEndY;
		unsigned lineWidth;
	};
	typedef std::vector<Region> Regions;
	Regions regions;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
