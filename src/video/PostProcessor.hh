// $Id$

#ifndef POSTPROCESSOR_HH
#define POSTPROCESSOR_HH

#include "RawFrame.hh"
#include "Scaler.hh"
#include "Deinterlacer.hh"
#include "Renderer.hh"
#include "VideoLayer.hh"
#include "Blender.hh"

struct SDL_Surface;


namespace openmsx {

/** Rasterizer using SDL.
  */
template <class Pixel, Renderer::Zoom zoom>
class PostProcessor : public VideoLayer
{
public:
	/** Constructor.
	  */
	PostProcessor(SDL_Surface* screen);

	/** Destructor.
	  */
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
	RawFrame* rotateFrames(RawFrame* finishedFrame, RawFrame::FieldType field);

private:
	/** Horizontal dimensions of the screen.
	  */
	static const unsigned WIDTH = (zoom == Renderer::ZOOM_256 ? 320 : 640);

	/** Vertical dimensions of the screen.
	  */
	static const unsigned HEIGHT = (zoom == Renderer::ZOOM_256 ? 240 : 480);

	/** Number of host screen lines per VDP display line.
	  */
	static const int LINE_ZOOM = (zoom == Renderer::ZOOM_256 ? 1 : 2);

	/** (Re)initialize the "abcdFrame" variables.
	  * @param first True iff this is the first call.
	  *   The first call should be done by the constructor.
	  */
	void initFrames(bool first = false);

	/** The currently active scaler.
	  */
	std::auto_ptr<Scaler<Pixel> > currScaler;

	/** ID of the currently active scaler.
	  * Used to detect scaler changes.
	  */
	ScalerID currScalerID;

	Deinterlacer<Pixel> deinterlacer;

	/** The surface which is visible to the user.
	  */
	SDL_Surface* screen;

	/** The last finished frame, ready to be displayed.
	  */
	RawFrame* currFrame;

	/** The frame before currFrame, ready to be displayed.
	  */
	RawFrame* prevFrame;

	/** Blend to pixel colors
	 */
	Blender<Pixel> blender;

};

} // namespace openmsx

#endif // POSTPROCESSOR_HH
