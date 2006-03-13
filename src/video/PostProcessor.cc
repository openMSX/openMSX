// $Id$

#include "PostProcessor.hh"
#include "Display.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RawFrame.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

PostProcessor::PostProcessor(CommandController& commandController,
	Display& display, VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(videoSource, commandController, display)
	, renderSettings(display.getRenderSettings())
	, screen(screen_)
{
	currFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	prevFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getFormat());
	interlacedFrame   = new DoubledFrame     (screen.getFormat());
}

PostProcessor::~PostProcessor()
{
	delete currFrame;
	delete prevFrame;
	delete deinterlacedFrame;
	delete interlacedFrame;
}

unsigned PostProcessor::getLineWidth(
	FrameSource* frame, unsigned y, unsigned step
	)
{
	unsigned result = frame->getLineWidth(y);
	for (unsigned i = 1; i < step; ++i) {
		result = std::max(result, frame->getLineWidth(y + i));
	}
	return result;
}

const std::string& PostProcessor::getName()
{
	static const std::string V99x8_NAME = "V99x8 PostProcessor";
	static const std::string V9990_NAME = "V9990 PostProcessor";
	return (getVideoSource() == VIDEO_GFX9000) ? V9990_NAME : V99x8_NAME;
}

RawFrame* PostProcessor::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field)
{
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->init(field);
	return reuseFrame;
}

} // namespace openmsx
