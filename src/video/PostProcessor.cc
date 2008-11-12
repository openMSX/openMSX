// $Id$

#include "PostProcessor.hh"
#include "Display.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "ScreenShotSaver.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "RawFrame.hh"
#include "AviRecorder.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

PostProcessor::PostProcessor(MSXMotherBoard& motherBoard,
	Display& display_, VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(motherBoard, videoSource, display_)
	, renderSettings(display_.getRenderSettings())
	, screen(screen_)
	, paintFrame(0)
	, recorder(0)
	, display(display_)
{
	currFrame = new RawFrame(screen.getSDLFormat(), maxWidth, height);
	prevFrame = new RawFrame(screen.getSDLFormat(), maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getSDLFormat());
	interlacedFrame   = new DoubledFrame     (screen.getSDLFormat());
}

PostProcessor::~PostProcessor()
{
	if (recorder) {
		display.getCliComm().printWarning(
			"Videorecording stopped, because you quit openMSX, "
			"changed machine, or changed a video setting "
			"during recording.");
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
	EmuTime::param time)
{
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
		const unsigned height = recorder->getFrameHeight();
		const void* lines[height];
		getScaledFrame(height, lines);
		/*for (unsigned i = 0; i < height; ++i) {
			if (getBpp() == 32) {
#if HAVE_32BPP
				// 32bpp
				if (height == 240) {
					lines[i] = paintFrame->getLinePtr320_240<unsigned>(i);
				} else {
					assert (height == 480);
					lines[i] = paintFrame->getLinePtr640_480<unsigned>(i);
				}
#endif
			} else {
#if HAVE_16BPP
				// 15bpp or 16bpp
				if (height == 240) {
					lines[i] = paintFrame->getLinePtr320_240<word>(i);
				} else {
					assert (height == 480);
					lines[i] = paintFrame->getLinePtr640_480<word>(i);
				}
#endif
			}
		}*/
		recorder->addImage(lines, time);
		paintFrame->freeLineBuffers();
	}

	return reuseFrame;
}

void PostProcessor::getScaledFrame(unsigned height, const void** lines)
{
	for (unsigned i = 0; i < height; ++i) {
		if (getBpp() == 32) {
#if HAVE_32BPP
			// 32bpp
			if (height == 240) {
				lines[i] = paintFrame->getLinePtr320_240<unsigned>(i);
			} else {
				assert (height == 480);
				lines[i] = paintFrame->getLinePtr640_480<unsigned>(i);
			}
#endif
		} else {
#if HAVE_16BPP
			// 15bpp or 16bpp
			if (height == 240) {
				lines[i] = paintFrame->getLinePtr320_240<word>(i);
			} else {
				assert (height == 480);
				lines[i] = paintFrame->getLinePtr640_480<word>(i);
			}
#endif
		}
	}
}

void PostProcessor::takeScreenShot(unsigned height, const std::string& filename)
{
	if (!paintFrame) {
		throw CommandException("TODO");
	}
	const void* lines[height];
	getScaledFrame(height, lines);

	unsigned width = (height == 240) ? 320 : 640;
	ScreenShotSaver::save(width, height, lines,
	                      paintFrame->getSDLPixelFormat(), filename);
	paintFrame->freeLineBuffers();
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
	return screen.getSDLFormat().BitsPerPixel;
}

} // namespace openmsx
