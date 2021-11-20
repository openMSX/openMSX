#include "DoubledFrame.hh"
#include <cstdint>

namespace openmsx {

DoubledFrame::DoubledFrame(const PixelFormat& format)
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

unsigned DoubledFrame::getLineWidth(unsigned line) const
{
	int t = line - skip;
	return (t >= 0) ? field->getLineWidth(t / 2) : 1;
}

const void* DoubledFrame::getLineInfo(
	unsigned line, unsigned& width, void* buf, unsigned bufWidth) const
{
	return field->getLineInfo(std::max<int>(line - skip, 0) / 2, width, buf, bufWidth);
}

} // namespace openmsx
