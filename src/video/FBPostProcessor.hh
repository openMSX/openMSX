// $Id$

#ifndef FBPOSTPROCESSOR_HH
#define FBPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "PixelOperations.hh"
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Scaler;
class Display;

/** Rasterizer using SDL.
  */
template <class Pixel>
class FBPostProcessor : public PostProcessor
{
public:
	FBPostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, VideoSource videoSource,
		unsigned maxWidth, unsigned height);
	virtual ~FBPostProcessor();

	// Layer interface:
	virtual void paint(OutputSurface& output);

	virtual RawFrame* rotateFrames(
		RawFrame* finishedFrame, FrameSource::FieldType field,
		EmuTime::param time);
	virtual void setSuperimposeFrame(const RawFrame* videoSource);

private:
	void preCalcNoise(double factor);
	void drawNoise(OutputSurface& output);
	void drawNoiseLine(Pixel* in, Pixel* out, signed char* noise,
	                   unsigned long width);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	/** The currently active scaler.
	  */
	std::auto_ptr<Scaler> currScaler;

	const RawFrame* superImposeFrame;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm;

	/** Currently active scale factor, used to detect scaler changes.
	  */
	unsigned scaleFactor;

	/** Remember the noise values to get a stable image when paused.
	 */
	std::vector<unsigned> noiseShift;

	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif // FBPOSTPROCESSOR_HH
