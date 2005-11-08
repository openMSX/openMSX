// $Id$

#ifndef DEINTERLACEDFRAME_HH
#define DEINTERLACEDFRAME_HH

#include "FrameSource.hh"
#include <cassert>

namespace openmsx {

/** Produces a deinterlaced video frame based on two RawFrames containing the
  * even and odd field.
  * This class does not copy the data from the RawFrames.
  */
class DeinterlacedFrame : public FrameSource
{
public:
	DeinterlacedFrame(FrameSource* evenField, FrameSource* oddField) {
		// TODO: I think these assertions make sense, but we cannot currently
		//       guarantee them. See TODO in PostProcessor::paint.
		//assert(evenField->getField() == RawFrame::FIELD_EVEN);
		//assert(oddField->getField() == RawFrame::FIELD_ODD);
		assert(evenField->height == oddField->height);
		height = 2 * evenField->getHeight();
		fields[0] = evenField;
		fields[1] = oddField;
	}

	virtual ~DeinterlacedFrame() {
		// TODO: Should DeinterlacedFrame become the owner of the even/odd
		//       frame?
	}

	virtual FieldType getField() {
		return FIELD_NONINTERLACED;
	}

	virtual unsigned getLineWidth(unsigned line) {
		return fields[line & 1]->getLineWidth(line >> 1);
	}

protected:
	virtual void* getLinePtrImpl(unsigned line) {
		return fields[line & 1]->getLinePtrImpl(line >> 1);
	}

private:
	/** The original frames whose data will be deinterlaced.
	  * The even frame is at index 0, the odd frame at index 1.
	  */
	FrameSource* fields[2];
};

} // namespace openmsx

#endif // DEINTERLACEDFRAME_HH
