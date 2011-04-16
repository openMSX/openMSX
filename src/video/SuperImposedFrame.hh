// $Id$

#ifndef SUPERIMPOSEDFRAME_HH
#define SUPERIMPOSEDFRAME_HH

#include "FrameSource.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** This class represents a frame that is the (per-pixel) alpha-blend of two
  * other frames. The resolution of the final frame is (per-line) the same
  * as the 1st input frame (exception: minimal line width is 320).
  */
template <typename Pixel>
class SuperImposedFrame : public FrameSource
{
public:
	SuperImposedFrame(const FrameSource& src, const FrameSource& super,
	                  const PixelOperations<Pixel>& pixelOps);

private:
	virtual const void* getLineInfo(unsigned line, unsigned& width) const;

	const FrameSource& src;
	const FrameSource& super;
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
