// $Id$

#include "PostProcessor.hh"
#include "Display.hh"
#include "OutputSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "SuperImposedFrame.hh"
#include "PNG.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "RawFrame.hh"
#include "AviRecorder.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "vla.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

PostProcessor::PostProcessor(MSXMotherBoard& motherBoard,
	Display& display_, OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth, unsigned height, bool canDoInterlace_)
	: VideoLayer(motherBoard, videoSource)
	, renderSettings(display_.getRenderSettings())
	, screen(screen_)
	, paintFrame(nullptr)
	, recorder(nullptr)
	, superImposeVideoFrame(nullptr)
	, superImposeVdpFrame(nullptr)
	, display(display_)
	, canDoInterlace(canDoInterlace_)
{
	if (canDoInterlace) {
		currFrame = make_unique<RawFrame>(
			screen.getSDLFormat(), maxWidth, height);
		prevFrame = make_unique<RawFrame>(
			screen.getSDLFormat(), maxWidth, height);
		deinterlacedFrame = make_unique<DeinterlacedFrame>(
			screen.getSDLFormat());
		interlacedFrame   = make_unique<DoubledFrame>(
			screen.getSDLFormat());
		superImposedFrame = SuperImposedFrame::create(
			screen.getSDLFormat());
	} else {
		// Laserdisc always produces non-interlaced frames, so we don't
		// need prevFrame, deinterlacedFrame and interlacedFrame. Also
		// it produces a complete frame at a time, so we don't need
		// currFrame (and have a separate work buffer, for partially
		// rendered frames).
	}
}

PostProcessor::~PostProcessor()
{
	if (recorder) {
		getCliComm().printWarning(
			"Videorecording stopped, because you "
			"changed machine or changed a video setting "
			"during recording.");
		recorder->stop();
	}
}

CliComm& PostProcessor::getCliComm()
{
	return display.getCliComm();
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

string_ref PostProcessor::getLayerName() const
{
	return "TODO"; // This breaks video recording!!
}

std::unique_ptr<RawFrame> PostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
	EmuTime::param time)
{
	std::unique_ptr<RawFrame> reuseFrame;
	if (canDoInterlace) {
		reuseFrame = std::move(prevFrame);
		prevFrame = std::move(currFrame);
		currFrame = std::move(finishedFrame);
		reuseFrame->init(field);
	} else {
		currFrame = std::move(finishedFrame);
		assert(field                 == FrameSource::FIELD_NONINTERLACED);
		assert(currFrame->getField() == FrameSource::FIELD_NONINTERLACED);
	}

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
				deinterlacedFrame->init(prevFrame.get(), currFrame.get());
			} else {
				deinterlacedFrame->init(currFrame.get(), prevFrame.get());
			}
			paintFrame = deinterlacedFrame.get();
		} else {
			// interlaced
			interlacedFrame->init(currFrame.get(),
				(currType == FrameSource::FIELD_ODD) ? 1 : 0);
			paintFrame = interlacedFrame.get();
		}
	} else {
		// non interlaced
		paintFrame = currFrame.get();
	}

	if (superImposeVdpFrame) {
		superImposedFrame->init(paintFrame, superImposeVdpFrame);
		paintFrame = superImposedFrame.get();
	}

	if (recorder && needRecord()) {
		recorder->addImage(paintFrame, time);
		paintFrame->freeLineBuffers();
	}

	if (canDoInterlace) {
		return reuseFrame;
	} else {
		return std::move(currFrame);
	}
}

void PostProcessor::setSuperimposeVideoFrame(const RawFrame* videoSource)
{
	superImposeVideoFrame = videoSource;
}

void PostProcessor::setSuperimposeVdpFrame(const FrameSource* vdpSource)
{
	superImposeVdpFrame = vdpSource;
}

void PostProcessor::getScaledFrame(unsigned height, const void** lines)
{
	for (unsigned i = 0; i < height; ++i) {
#if HAVE_32BPP
		if (getBpp() == 32) {
			// 32bpp
			if (height == 240) {
				lines[i] = paintFrame->getLinePtr320_240<unsigned>(i);
			} else {
				assert (height == 480);
				lines[i] = paintFrame->getLinePtr640_480<unsigned>(i);
			}
		} else
#endif
		{
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

void PostProcessor::takeRawScreenShot(unsigned height, const std::string& filename)
{
	if (!paintFrame) {
		throw CommandException("TODO");
	}
	VLA(const void*, lines, height);
	getScaledFrame(height, lines);

	unsigned width = (height == 240) ? 320 : 640;
	PNG::save(width, height, lines, paintFrame->getSDLPixelFormat(), filename);
	paintFrame->freeLineBuffers();
}

void PostProcessor::setRecorder(AviRecorder* recorder_)
{
	recorder = recorder_;
}

bool PostProcessor::isRecording() const
{
	return recorder != nullptr;
}

unsigned PostProcessor::getBpp() const
{
	return screen.getSDLFormat().BitsPerPixel;
}

} // namespace openmsx
