// $Id$

#include "PostProcessor.hh"
#include "Display.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "RawFrame.hh"
#include "AviRecorder.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

PostProcessor::PostProcessor(CommandController& commandController,
	Display& display, VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(videoSource, commandController, display)
	, renderSettings(display.getRenderSettings())
	, screen(screen_)
	, paintFrame(0)
	, recorder(0)
	, prevTime(EmuTime::zero)
{
	currFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	prevFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getFormat());
	interlacedFrame   = new DoubledFrame     (screen.getFormat());
}

PostProcessor::~PostProcessor()
{
	if (recorder) {
		recorder->stop();
	}
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
	RawFrame* finishedFrame, FrameSource::FieldType field,
	const EmuTime& time)
{
	lastFrameDuration = time - prevTime;
	prevTime = time;

	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->init(field);

	// TODO: When frames are being skipped or if (de)interlace was just
	//       turned on, it's not guaranteed that prevFrame is a
	//       different field from currFrame.
	//       Or in the case of frame skip, it might be the right field,
	//       but from several frames ago.
	FrameSource::FieldType currType = currFrame->getField();
	if (currType != FrameSource::FIELD_NONINTERLACED) {
		if (renderSettings.getDeinterlace().getValue()) {
			// deinterlaced
			if (currType == FrameSource::FIELD_ODD) {
				deinterlacedFrame->init(prevFrame, currFrame);
			} else {
				deinterlacedFrame->init(currFrame, prevFrame);
			}
			paintFrame = deinterlacedFrame;
		} else {
			// interlaced
			interlacedFrame->init(currFrame,
				(currType == FrameSource::FIELD_ODD) ? 1 : 0);
			paintFrame = interlacedFrame;
		}
	} else {
		// non interlaced
		paintFrame = currFrame;
	}

	if (recorder) {
		const void* lines[240];
		for (int i = 0; i < 240; ++i) {
			if (getBpp() == 32) {
				lines[i] = paintFrame->getLinePtr320_240(i, (unsigned*)0);
			} else {
				lines[i] = paintFrame->getLinePtr320_240(i, (word*)0);
			}
		}
		recorder->addImage(lines);
		finishedFrame->freeLineBuffers();
	}

	return reuseFrame;
}

void PostProcessor::setRecorder(AviRecorder* recorder_)
{
	recorder = recorder_;
}

bool PostProcessor::isRecording() const
{
	return recorder;
}

unsigned PostProcessor::getBpp() const
{
	return screen.getFormat()->BitsPerPixel;
}

double PostProcessor::getLastFrameDuration() const
{
	return lastFrameDuration.toDouble();
}

} // namespace openmsx
