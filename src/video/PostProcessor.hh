// $Id$

#ifndef POSTPROCESSOR_HH
#define POSTPROCESSOR_HH

#include "FrameSource.hh"
#include "VideoLayer.hh"

namespace openmsx {

class CommandController;
class Display;
class RenderSettings;
class VisibleSurface;
class RawFrame;
class DeinterlacedFrame;
class DoubledFrame;

/** Abstract base class for post processors.
  * A post processor builds the frame that is displayed from the MSX frame,
  * while applying effects such as scalers, noise etc.
  * TODO: With some refactoring, it would be possible to move much or even all
  *       of the post processing code here instead of in the subclasses.
  */
class PostProcessor : public VideoLayer
{
public:
	virtual ~PostProcessor();

	// Layer interface:
	virtual void paint() = 0;
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
	virtual RawFrame* rotateFrames(
		RawFrame* finishedFrame, FrameSource::FieldType field
		);

protected:
	/** Returns the maximum width for lines [y..y+step).
	  */
	static unsigned getLineWidth(FrameSource* frame, unsigned y, unsigned step);

	PostProcessor(
		CommandController& commandController, Display& display,
		VisibleSurface& screen, VideoSource videoSource,
		unsigned maxWidth, unsigned height
		);

	/** Render settings
	  */
	RenderSettings& renderSettings;

	/** The surface which is visible to the user.
	  */
	VisibleSurface& screen;

	/** The last finished frame, ready to be displayed.
	  */
	RawFrame* currFrame;

	/** The frame before currFrame, ready to be displayed.
	  */
	RawFrame* prevFrame;

	/** Combined currFrame and prevFrame.
	  */
	DeinterlacedFrame* deinterlacedFrame;

	/** Each line of currFrame twice, to get double vertical resolution.
	  */
	DoubledFrame* interlacedFrame;
};

} // namespace openmsx

#endif // POSTPROCESSOR_HH
