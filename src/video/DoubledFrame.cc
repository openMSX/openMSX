#include "DoubledFrame.hh"
#include "narrow.hh"
#include <cstdint>

namespace openmsx {

void DoubledFrame::init(FrameSource* field_, int skip_)
{
	FrameSource::init(FIELD_NONINTERLACED);
	field = field_;
	skip = skip_;
	setHeight(2 * field->getHeight());
}

unsigned DoubledFrame::getLineWidth(unsigned line) const
{
	int t = narrow<int>(line) - skip;
	return (t >= 0) ? field->getLineWidth(t / 2) : 1;
}

std::span<const FrameSource::Pixel> DoubledFrame::getUnscaledLine(
	unsigned line, void* buf, unsigned bufWidth) const
{
	return field->getUnscaledLine(std::max(narrow<int>(line) - skip, 0) / 2, buf, bufWidth);
}

} // namespace openmsx
