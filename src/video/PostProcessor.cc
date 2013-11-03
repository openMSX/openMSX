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
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "CommandException.hh"
#include "MemoryOps.hh"
#include "vla.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace openmsx {

PostProcessor::PostProcessor(MSXMotherBoard& motherBoard,
	Display& display_, OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth, unsigned height, bool canDoInterlace_)
	: VideoLayer(motherBoard, videoSource)
	, Schedulable(motherBoard.getScheduler())
	, renderSettings(display_.getRenderSettings())
	, screen(screen_)
	, paintFrame(nullptr)
	, recorder(nullptr)
	, superImposeVideoFrame(nullptr)
	, superImposeVdpFrame(nullptr)
	, interleaveCount(0)
	, display(display_)
	, canDoInterlace(canDoInterlace_)
	, lastRotate(motherBoard.getCurrentTime())
	, eventDistributor(motherBoard.getReactor().getEventDistributor())
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
	FrameSource* frame, unsigned y, unsigned step)
{
	unsigned result = frame->getLineWidth(y);
	for (unsigned i = 1; i < step; ++i) {
		result = std::max(result, frame->getLineWidth(y + i));
	}
	return result;
}

std::unique_ptr<RawFrame> PostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
	EmuTime::param time)
{
	if (renderSettings.getInterleaveBlackFrame().getBoolean()) {
		auto delta = time - lastRotate; // time between last two calls
		auto middle = time + delta / 2; // estimate for middle between now
		                                // and next call
		setSyncPoint(middle);
	}
	lastRotate = time;

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
		if (renderSettings.getDeinterlace().getBoolean()) {
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
	}

	if (canDoInterlace) {
		return reuseFrame;
	} else {
		return std::move(currFrame);
	}
}

void PostProcessor::executeUntil(EmuTime::param /*time*/, int /*userData*/)
{
	// insert fake end of frame event
	eventDistributor.distributeEvent(
		std::make_shared<FinishFrameEvent>(
			getVideoSource(), getVideoSourceSetting(), false));
}

void PostProcessor::setSuperimposeVideoFrame(const RawFrame* videoSource)
{
	superImposeVideoFrame = videoSource;
}

void PostProcessor::setSuperimposeVdpFrame(const FrameSource* vdpSource)
{
	superImposeVdpFrame = vdpSource;
}

void PostProcessor::getScaledFrame(unsigned height, const void** lines,
                                   std::vector<void*>& workBuffer)
{
	unsigned width = (height == 240) ? 320 : 640;
	unsigned pitch = width * ((getBpp() == 32) ? 4 : 2);
	const void* line = nullptr;
	void* work = nullptr;
	for (unsigned i = 0; i < height; ++i) {
		if (line == work) {
			// If work buffer was used in previous iteration,
			// then allocate a new one.
			work = MemoryOps::mallocAligned(16, pitch);
			workBuffer.push_back(work);
		}
#if HAVE_32BPP
		if (getBpp() == 32) {
			// 32bpp
			auto* work2 = static_cast<uint32_t*>(work);
			if (height == 240) {
				line = paintFrame->getLinePtr320_240(i, work2);
			} else {
				assert (height == 480);
				line = paintFrame->getLinePtr640_480(i, work2);
			}
		} else
#endif
		{
#if HAVE_16BPP
			// 15bpp or 16bpp
			auto* work2 = static_cast<uint16_t*>(work);
			if (height == 240) {
				line = paintFrame->getLinePtr320_240(i, work2);
			} else {
				assert (height == 480);
				line = paintFrame->getLinePtr640_480(i, work2);
			}
#endif
		}
		lines[i] = line;
	}
}

void PostProcessor::takeRawScreenShot(unsigned height, const std::string& filename)
{
	if (!paintFrame) {
		throw CommandException("TODO");
	}

	VLA(const void*, lines, height);
	std::vector<void*> workBuffer;
	getScaledFrame(height, lines, workBuffer);

	unsigned width = (height == 240) ? 320 : 640;
	PNG::save(width, height, lines, paintFrame->getSDLPixelFormat(), filename);

	for (void* p : workBuffer) {
		MemoryOps::freeAligned(p);
	}
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
