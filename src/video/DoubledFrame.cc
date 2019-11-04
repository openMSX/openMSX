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
	static constexpr uint32_t blackPixel = 0; // both 16bppp and 32bpp
	int t = line - skip;
	if (t >= 0) {
		return field->getLineInfo(t / 2, width, buf, bufWidth);
	} else {
		width = 1;
		return &blackPixel;
	}
}

} // namespace openmsx
