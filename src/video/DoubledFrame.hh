// $Id$

#ifndef DOUBLEDFRAME_HH
#define DOUBLEDFRAME_HH

#include "FrameSource.hh"

namespace openmsx {

/** Produces a video frame that has every line from the input frame twice
  * plus a number of black lines at the top.
  * This class does not copy the data from the input FrameSource.
  */
class DoubledFrame : public FrameSource
{
public:
	DoubledFrame(const SDL_PixelFormat* format);

	void init(FrameSource* field, unsigned skip);

	virtual unsigned getLineBufferSize() const;
	virtual unsigned getLineWidth(unsigned line);

protected:
	virtual void* getLinePtrImpl(unsigned line);

private:
	/** The original frame whose data will be doubled.
	  */
	FrameSource* field;
	unsigned skip;
};

} // namespace openmsx

#endif
