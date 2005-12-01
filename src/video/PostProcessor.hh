// $Id$

#ifndef POSTPROCESSOR_HH
#define POSTPROCESSOR_HH

#include "FrameSource.hh"
#include "Scaler.hh"
#include "VideoLayer.hh"

namespace openmsx {

class CommandController;
class RenderSettings;
class Display;
class OutputSurface;
class RawFrame;
class DeinterlacedFrame;
class DoubledFrame;

/** Rasterizer using SDL.
  */
template <class Pixel>
class PostProcessor : public VideoLayer
{
public:
	PostProcessor(CommandController& commandController,
	              RenderSettings& renderSettings, Display& display,
	              OutputSurface& screen, VideoSource videoSource,
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

	void paintBlank(
		FrameSource& frame, unsigned srcStep, unsigned dstStep,
		unsigned srcStartY, unsigned srcEndY,
		OutputSurface& screen, unsigned dstStartY, unsigned dstHeight);

	/** Render settings
	  */
	RenderSettings& renderSettings;

	/** The currently active scaler.
	  */
	std::auto_ptr<Scaler<Pixel> > currScaler;

	/** ID of the currently active scaler.
	  * Used to detect scaler changes.
	  */
	ScalerID currScalerID;

	/** The surface which is visible to the user.
	  */
	OutputSurface& screen;

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
};

} // namespace openmsx

#endif // POSTPROCESSOR_HH
