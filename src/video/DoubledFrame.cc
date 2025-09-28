#include "DoubledFrame.hh"

#include "narrow.hh"

namespace openmsx {

void DoubledFrame::init(FrameSource* field_, int skip_)
{
	FrameSource::init(FieldType::NONINTERLACED);
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
	unsigned line, std::span<Pixel> helpBuf) const
{
	return field->getUnscaledLine(std::max(narrow<int>(line) - skip, 0) / 2, helpBuf);
}

} // namespace openmsx
