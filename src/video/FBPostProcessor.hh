#ifndef FBPOSTPROCESSOR_HH
#define FBPOSTPROCESSOR_HH

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "PixelOperations.hh"
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Display;
template<typename Pixel> class Scaler;

/** Rasterizer using SDL.
  */
template <class Pixel>
class FBPostProcessor : public PostProcessor
{
public:
	FBPostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);
	virtual ~FBPostProcessor();

	// Layer interface:
	virtual void paint(OutputSurface& output);

	virtual std::unique_ptr<RawFrame> rotateFrames(
		std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
		EmuTime::param time);

private:
	void preCalcNoise(double factor);
	void drawNoise(OutputSurface& output);
	void drawNoiseLine(Pixel* buf, signed char* noise,
	                   unsigned long width);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	/** The currently active scaler.
	  */
	std::unique_ptr<Scaler<Pixel>> currScaler;

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
