// $Id$

#include "DoubledFrame.hh"

namespace openmsx {

DoubledFrame::DoubledFrame(const SDL_PixelFormat* format)
	: FrameSource(format)
{
}

void DoubledFrame::init(FrameSource* field_, unsigned skip_)
{
	FrameSource::init(FIELD_NONINTERLACED);
	field = field_;
	skip = skip_;
	setHeight(2 * field->getHeight());
}

unsigned DoubledFrame::getLineBufferSize() const
{
	return field->getLineBufferSize();
}

unsigned DoubledFrame::getLineWidth(unsigned line)
{
	int t = line - skip;
	return (t >= 0) ? field->getLineWidth(t / 2) : 1;
}

void* DoubledFrame::getLinePtrImpl(unsigned line)
{
	static unsigned blackPixel = 0; // both 16bppp and 32bpp
	int t = line - skip;
	return (t >= 0) ? field->getLinePtrImpl(t / 2) : &blackPixel;
}

} // namespace openmsx

