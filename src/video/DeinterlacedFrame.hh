// $Id$

#ifndef DEINTERLACEDFRAME_HH
#define DEINTERLACEDFRAME_HH

#include "FrameSource.hh"
#include <cassert>

namespace openmsx {

/** Produces a deinterlaced video frame based on two other FrameSources
  * (typically two RawFrames) containing the even and odd field.
  * This class does not copy the data from the input FrameSources.
  */
class DeinterlacedFrame : public FrameSource
{
public:
	DeinterlacedFrame(const SDL_PixelFormat* format);

	void init(FrameSource* evenField, FrameSource* oddField);

	virtual unsigned getLineBufferSize() const;
	virtual unsigned getLineWidth(unsigned line);

protected:
	virtual void* getLinePtrImpl(unsigned line);

private:
	/** The original frames whose data will be deinterlaced.
	  * The even frame is at index 0, the odd frame at index 1.
	  */
	FrameSource* fields[2];
};

} // namespace openmsx

#endif // DEINTERLACEDFRAME_HH
