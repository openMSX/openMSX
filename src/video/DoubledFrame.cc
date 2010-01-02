// $Id$

#include "DoubledFrame.hh"

namespace openmsx {

DoubledFrame::DoubledFrame(const SDL_PixelFormat& format)
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

const void* DoubledFrame::getLineInfo(unsigned line, unsigned& width) const
{
	static const unsigned blackPixel = 0; // both 16bppp and 32bpp
	int t = line - skip;
	if (t >= 0) {
		return field->getLineInfo(t / 2, width);
	} else {
		width = 1;
		return &blackPixel;
	}
}

} // namespace openmsx
