#include "PostProcessor.hh"
#include "Display.hh"
#include "OutputSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "Deflicker.hh"
#include "SuperImposedFrame.hh"
#include "PNG.hh"
#include "RenderSettings.hh"
#include "RawFrame.hh"
#include "AviRecorder.hh"
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "CommandException.hh"
#include "MemBuffer.hh"
#include "aligned.hh"
#include "narrow.hh"
#include "vla.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>

namespace openmsx {

PostProcessor::PostProcessor(MSXMotherBoard& motherBoard_,
	Display& display_, OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth_, unsigned height_, bool canDoInterlace_)
	: VideoLayer(motherBoard_, videoSource)
	, Schedulable(motherBoard_.getScheduler())
	, renderSettings(display_.getRenderSettings())
	, screen(screen_)
	, maxWidth(maxWidth_)
	, height(height_)
	, display(display_)
	, canDoInterlace(canDoInterlace_)
	, lastRotate(motherBoard_.getCurrentTime())
	, eventDistributor(motherBoard_.getReactor().getEventDistributor())
{
	if (canDoInterlace) {
		deinterlacedFrame = std::make_unique<DeinterlacedFrame>(
			screen.getPixelFormat());
		interlacedFrame   = std::make_unique<DoubledFrame>(
			screen.getPixelFormat());
		deflicker = Deflicker::create(
			screen.getPixelFormat(), lastFrames);
		superImposedFrame = SuperImposedFrame::create(
			screen.getPixelFormat());
	} else {
		// Laserdisc always produces non-interlaced frames, so we don't
		// need lastFrames[1..3], deinterlacedFrame and
		// interlacedFrame. Also it produces a complete frame at a
		// time, so we don't need lastFrames[0] (and have a separate
		// work buffer, for partially rendered frames).
	}
}

PostProcessor::~PostProcessor()
{
	if (recorder) {
		getCliComm().printWarning(
			"Video recording stopped, because you "
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
	return max_value(xrange(step), [&](auto i) { return frame->getLineWidth(y + i); });
}

std::unique_ptr<RawFrame> PostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time)
{
	if (renderSettings.getInterleaveBlackFrame()) {
		auto delta = time - lastRotate; // time between last two calls
		auto middle = time + delta / 2; // estimate for middle between now
		                                // and next call
		setSyncPoint(middle);
	}
	lastRotate = time;

	// Figure out how many past frames we want to use.
	int numRequired = 1;
	bool doDeinterlace = false;
	bool doInterlace   = false;
	bool doDeflicker   = false;
	auto currType = finishedFrame->getField();
	if (canDoInterlace) {
		if (currType != FrameSource::FIELD_NONINTERLACED) {
			if (renderSettings.getDeinterlace()) {
				doDeinterlace = true;
				numRequired = 2;
			} else {
				doInterlace = true;
			}
		} else if (renderSettings.getDeflicker()) {
			doDeflicker = true;
			numRequired = 4;
		}
	}

	// Which frame can be returned (recycled) to caller. Prefer to return
	// the youngest frame to improve cache locality.
	int recycleIdx = (lastFramesCount < numRequired)
		? lastFramesCount++  // store one more
		: (numRequired - 1); // youngest that's no longer needed
	assert(recycleIdx < 4);
	auto recycleFrame = std::move(lastFrames[recycleIdx]); // might be nullptr

	// Insert new frame in front of lastFrames[], shift older frames
	std::move_backward(&lastFrames[0], &lastFrames[recycleIdx],
	                   &lastFrames[recycleIdx + 1]);
	lastFrames[0] = std::move(finishedFrame);

	// Are enough frames available?
	if (lastFramesCount >= numRequired) {
		// Only the last 'numRequired' are kept up to date.
		lastFramesCount = numRequired;
	} else {
		// Not enough past frames, fall back to 'regular' rendering.
		// This situation can only occur when:
		// - The very first frame we render needs to be deinterlaced.
		//   In other case we have at least one valid frame from the
		//   past plus one new frame passed via the 'finishedFrame'
		//   parameter.
		// - Or when (re)enabling the deflicker setting. Typically only
		//   1 frame in lastFrames[] is kept up-to-date (and we're
		//   given 1 new frame), so it can take up-to 2 frame after
		//   enabling deflicker before it actually takes effect.
		doDeinterlace = false;
		doInterlace   = false;
		doDeflicker   = false;
	}

	// Setup the to-be-painted frame
	if (doDeinterlace) {
		if (currType == FrameSource::FIELD_ODD) {
			deinterlacedFrame->init(lastFrames[1].get(), lastFrames[0].get());
		} else {
			deinterlacedFrame->init(lastFrames[0].get(), lastFrames[1].get());
		}
		paintFrame = deinterlacedFrame.get();
	} else if (doInterlace) {
		interlacedFrame->init(
			lastFrames[0].get(),
			(currType == FrameSource::FIELD_ODD) ? 1 : 0);
		paintFrame = interlacedFrame.get();
	} else if (doDeflicker) {
		deflicker->init();
		paintFrame = deflicker.get();
	} else {
		paintFrame = lastFrames[0].get();
	}
	if (superImposeVdpFrame) {
		superImposedFrame->init(paintFrame, superImposeVdpFrame);
		paintFrame = superImposedFrame.get();
	}

	// Possibly record this frame
	if (recorder && needRecord()) {
		try {
			recorder->addImage(paintFrame, time);
		} catch (MSXException& e) {
			getCliComm().printWarning(
				"Recording stopped with error: ",
				e.getMessage());
			recorder->stop();
			assert(!recorder);
		}
	}

	// Return recycled frame to the caller
	if (canDoInterlace) {
		if (!recycleFrame) [[unlikely]] {
			recycleFrame = std::make_unique<RawFrame>(
				screen.getPixelFormat(), maxWidth, height);
		}
		return recycleFrame;
	} else {
		return std::move(lastFrames[0]);
	}
}

void PostProcessor::executeUntil(EmuTime::param /*time*/)
{
	// insert fake end of frame event
	eventDistributor.distributeEvent(
		Event::create<FinishFrameEvent>(
			getVideoSource(), getVideoSourceSetting(), false));
}

using WorkBuffer = std::vector<MemBuffer<char, SSE_ALIGNMENT>>;
static void getScaledFrame(FrameSource& paintFrame,
                           std::span<const void*> lines,
                           WorkBuffer& workBuffer)
{
	auto height = narrow<unsigned>(lines.size());
	unsigned width = (height == 240) ? 320 : 640;
	unsigned pitch = width * 4;
	const void* linePtr = nullptr;
	void* work = nullptr;
	for (auto i : xrange(height)) {
		if (linePtr == work) {
			// If work buffer was used in previous iteration,
			// then allocate a new one.
			work = workBuffer.emplace_back(pitch).data();
		}
		auto* work2 = static_cast<uint32_t*>(work);
		if (height == 240) {
			auto line = paintFrame.getLinePtr320_240(i, std::span<uint32_t, 320>{work2, 320});
			linePtr = line.data();
		} else {
			assert (height == 480);
			auto line = paintFrame.getLinePtr640_480(i, std::span<uint32_t, 640>{work2, 640});
			linePtr = line.data();
		}
		lines[i] = linePtr;
	}
}

void PostProcessor::takeRawScreenShot(unsigned height2, const std::string& filename)
{
	if (!paintFrame) {
		throw CommandException("TODO");
	}

	VLA(const void*, lines, height2);
	WorkBuffer workBuffer;
	getScaledFrame(*paintFrame, lines, workBuffer);
	unsigned width = (height2 == 240) ? 320 : 640;
	PNG::save(width, lines, paintFrame->getPixelFormat(), filename);
}

} // namespace openmsx
