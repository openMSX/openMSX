// $Id$

#include "DeinterlacedFrame.hh"

namespace openmsx {

DeinterlacedFrame::DeinterlacedFrame(const SDL_PixelFormat* format)
	: FrameSource(format)
{
}

void DeinterlacedFrame::init(FrameSource* evenField, FrameSource* oddField)
{
	FrameSource::init(FIELD_NONINTERLACED);
	// TODO: I think these assertions make sense, but we cannot currently
	//       guarantee them. See TODO in PostProcessor::paint.
	//assert(evenField->getField() == FrameSource::FIELD_EVEN);
	//assert(oddField->getField() == FrameSource::FIELD_ODD);
	assert(evenField->height == oddField->height);
	assert(evenField->getLineBufferSize() == oddField->getLineBufferSize());
	height = 2 * evenField->getHeight();
	fields[0] = evenField;
	fields[1] = oddField;
}

unsigned DeinterlacedFrame::getLineBufferSize() const
{
	return fields[0]->getLineBufferSize();
}

unsigned DeinterlacedFrame::getLineWidth(unsigned line)
{
	return fields[line & 1]->getLineWidth(line >> 1);
}

void* DeinterlacedFrame::getLinePtrImpl(unsigned line)
{
	return fields[line & 1]->getLinePtrImpl(line >> 1);
}

} // namespace openmsx

