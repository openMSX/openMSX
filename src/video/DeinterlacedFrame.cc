#include "DeinterlacedFrame.hh"
#include <cassert>

namespace openmsx {

void DeinterlacedFrame::init(FrameSource* evenField, FrameSource* oddField)
{
	FrameSource::init(FIELD_NONINTERLACED);
	// TODO: I think these assertions make sense, but we cannot currently
	//       guarantee them. See TODO in PostProcessor::paint.
	//assert(evenField->getField() == FrameSource::FIELD_EVEN);
	//assert(oddField->getField() == FrameSource::FIELD_ODD);
	assert(evenField->getHeight() == oddField->getHeight());
	setHeight(2 * evenField->getHeight());
	fields[0] = evenField;
	fields[1] = oddField;
}

unsigned DeinterlacedFrame::getLineWidth(unsigned line) const
{
	return fields[line & 1]->getLineWidth(line >> 1);
}

std::span<const FrameSource::Pixel> DeinterlacedFrame::getUnscaledLine(
	unsigned line, std::span<Pixel> helpBuf) const
{
	return fields[line & 1]->getUnscaledLine(line >> 1, helpBuf);
}

} // namespace openmsx
