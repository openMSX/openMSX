// $Id$

#ifndef LDRASTERIZER_HH
#define LDRASTERIZER_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class LDRasterizer
{
public:
	virtual ~LDRasterizer() {}

	/** Will the output of this Rasterizer be displayed?
	  * There is no point in producing a frame that will not be displayed.
	  * TODO: Is querying the next pipeline step the best way to solve this,
	  *       or is it better to explicitly disable the first step in the pipeline?
	  */
	virtual bool isActive() = 0;

	/** Resynchronize with VDP: all cached states are flushed.
	  */
	virtual void reset() = 0;

	/** Indicates the start of a new frame.
	  * The rasterizer can fetch per-frame settings from the VDP.
	  */
	virtual void frameStart(EmuTime::param time) = 0;

	/** Indicates the end of the current frame.
	  * The rasterizer can perform image post processing.
	  */
	virtual void frameEnd() = 0;

	/** Is video recording active?
	  */
	virtual bool isRecording() const = 0;

	virtual void drawBlank(int r, int g, int b) = 0;

	virtual void drawBitmap(const byte* frame) = 0;
};

} // namespace openmsx

#endif
