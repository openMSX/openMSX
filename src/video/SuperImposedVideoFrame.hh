// $Id$

#ifndef SUPERIMPOSEDVIDEOFRAME_HH
#define SUPERIMPOSEDVIDEOFRAME_HH

#include "FrameSource.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** This class represents a frame that is the (per-pixel) alpha-blend of a
  * (laser-disc) video frame and a V99x8 (or tms9918) video frame. This is
  * different from a generic 'SuperImposedFrame' class because it always
  * outputs the resolution of the MSX frame. Except for the top/bottom
  * border line, there we return a line with width=320.
  * So usually this means the laserdisc video gets downscaled to 320x240
  * resolution. The rational for this was that we won't the scalers to work
  * on the proper MSX resolution. So the MSX graphics get scaled in the same
  * way whether superimpose is enabled or not.
  */
template <typename Pixel>
class SuperImposedVideoFrame : public FrameSource
{
public:
	SuperImposedVideoFrame(const FrameSource& src, const FrameSource& super,
	                       const PixelOperations<Pixel>& pixelOps);

	virtual unsigned getLineWidth(unsigned line) const;
	virtual const void* getLineInfo(unsigned line, unsigned& width) const;

private:
	const FrameSource& src;
	const FrameSource& super;
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
