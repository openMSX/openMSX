// $Id$

#ifndef POSTPROCESSOR_HH
#define POSTPROCESSOR_HH

#include "RenderSettings.hh"
#include "FrameSource.hh"
#include "VideoLayer.hh"
#include <vector>

namespace openmsx {

class Scaler;
class CommandController;
class Display;
class VisibleSurface;
class RawFrame;
class DeinterlacedFrame;
class DoubledFrame;

/** Rasterizer using SDL.
  */
template <class Pixel>
class PostProcessor : public VideoLayer
{
public:
	PostProcessor(CommandController& commandController, Display& display,
	              VisibleSurface& screen, VideoSource videoSource,
	              unsigned maxWidth, unsigned height);
	virtual ~PostProcessor();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

	/** Sets up the "abcdFrame" variables for a new frame.
	  * TODO: The point of passing the finished frame in and the new workFrame
	  *       out is to be able to split off the scaler application as a
	  *       separate class.
	  * @param finishedFrame Frame that has just become available.
	  * @param field Specifies what role (if any) the new frame plays in
	  *   interlacing.
	  * @return RawFrame object that can be used for building the next frame.
	  */
	RawFrame* rotateFrames(RawFrame* finishedFrame, FrameSource::FieldType field);

private:
	/** (Re)initialize the "abcdFrame" variables.
	  * @param first True iff this is the first call.
	  *   The first call should be done by the constructor.
	  */
	void initFrames(bool first = false);

	void drawNoise();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	/** Render settings
	  */
	RenderSettings& renderSettings;

	/** The currently active scaler.
	  */
	std::auto_ptr<Scaler> currScaler;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm;

	/** Currently active scale factor, used to detect scaler changes.
	  */
	unsigned scaleFactor;

	/** The surface which is visible to the user.
	  */
	VisibleSurface& screen;

	/** The last finished frame, ready to be displayed.
	  */
	RawFrame* currFrame;

	/** The frame before currFrame, ready to be displayed.
	  */
	RawFrame* prevFrame;

	/** Combined currFrame and prevFrame
	  */
	DeinterlacedFrame* deinterlacedFrame;
	DoubledFrame* interlacedFrame;

	/** Remember the noise values to get a stable image when paused.
	 */
	std::vector<unsigned> noiseShift;
};

} // namespace openmsx

#endif // POSTPROCESSOR_HH
