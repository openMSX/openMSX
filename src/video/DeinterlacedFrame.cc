// $Id$

#include "DeinterlacedFrame.hh"
#include <cassert>

namespace openmsx {

DeinterlacedFrame::DeinterlacedFrame(const SDL_PixelFormat& format)
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
	assert(evenField->getHeight() == oddField->getHeight());
	assert(evenField->getLineBufferSize() == oddField->getLineBufferSize());
	setHeight(2 * evenField->getHeight());
	fields[0] = evenField;
	fields[1] = oddField;
}

unsigned DeinterlacedFrame::getLineBufferSize() const
{
	return fields[0]->getLineBufferSize();
}

const void* DeinterlacedFrame::getLineInfo(unsigned line, unsigned& width) const
{
	return fields[line & 1]->getLineInfo(line >> 1, width);
}

} // namespace openmsx
