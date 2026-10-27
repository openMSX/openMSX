#include "SMSVDPPixelRenderer.hh"

#include "SMSVDP.hh"
#include "SMSVDPRasterizer.hh"

#include "Display.hh"
#include "PostProcessor.hh"
#include "Reactor.hh"
#include "VideoSystem.hh"

#include "Event.hh"
#include "EventDistributor.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "RenderSettings.hh"
#include "SpeedManager.hh"
#include "ThrottleManager.hh"
#include "Timer.hh"
#include "VideoSourceSetting.hh"

#include "narrow.hh"
#include "one_of.hh"

#include <cassert>
#include <cmath>

namespace openmsx {

SMSVDPPixelRenderer::SMSVDPPixelRenderer(SMSVDP& vdp)
	: motherboard(vdp.getMotherBoard())
	, eventDistributor(motherboard.getReactor().getEventDistributor())
	, videoSourceSetting(motherboard.getVideoSource())
	, realTime(motherboard.getRealTime())
	, speedManager(
		motherboard.getReactor().getGlobalSettings().getSpeedManager())
	, throttleManager(
		motherboard.getReactor().getGlobalSettings().getThrottleManager())
	, renderSettings(motherboard.getReactor().getDisplay().getRenderSettings())
	, display(motherboard.getReactor().getDisplay())
	, rasterizer(display.getVideoSystem().createSMSVDPRasterizer(vdp))
{
	renderSettings.getMaxFrameSkipSetting().attach(*this);
	renderSettings.getMinFrameSkipSetting().attach(*this);
}

SMSVDPPixelRenderer::~SMSVDPPixelRenderer()
{
	renderSettings.getMinFrameSkipSetting().detach(*this);
	renderSettings.getMaxFrameSkipSetting().detach(*this);
}

PostProcessor* SMSVDPPixelRenderer::getPostProcessor() const
{
	return rasterizer->getPostProcessor();
}

bool SMSVDPPixelRenderer::isActive() const
{
	return renderFrame &&
	       rasterizer->getPostProcessor()->needRender() &&
	       motherboard.isActive() &&
	       !motherboard.isFastForwarding();
}

void SMSVDPPixelRenderer::frameStart(EmuTime time)
{
	// Frameskip, like PixelRenderer (the MSX VDP renderer): the emulation
	// speed is throttled to the speed of the painted frames, so without
	// skipping frames the emulation can never run faster than the display
	// refresh rate (e.g. during fast-forward).
	if (!rasterizer->getPostProcessor()->needRender() ||
	    !motherboard.isActive() ||
	    motherboard.isFastForwarding()) {
		frameSkipCounter = 999.0f;
		renderFrame = false;
		paintFrame = false;
		return;
	}

	if (throttleManager.isThrottled()) {
		// Note: min/maxFrameSkip control the number of skipped frames, but
		//       for every series of skipped frames there is also one painted
		//       frame, so our boundary checks are offset by one.
		auto counter = narrow_cast<int>(frameSkipCounter);
		if (counter < renderSettings.getMinFrameSkip()) {
			paintFrame = false;
		} else if (counter > renderSettings.getMaxFrameSkip()) {
			paintFrame = true;
		} else {
			paintFrame = realTime.timeLeft(
				unsigned(finishFrameDuration), time);
		}
		frameSkipCounter += 1.0f / float(speedManager.getSpeed());
	} else {
		// We need to render a frame every now and then,
		// to show the user what is happening.
		paintFrame = (Timer::getTime() - lastPaintTime) >= 100000; // 10 fps
	}

	if (paintFrame) {
		frameSkipCounter = std::remainder(frameSkipCounter, 1.0f);
	} else if (!rasterizer->getPostProcessor()->isRecording() &&
	           !display.isRenderAllFramesRequested()) {
		renderFrame = false;
		return;
	}
	renderFrame = true;
}

void SMSVDPPixelRenderer::frameEnd(EmuTime time)
{
	if (renderFrame) {
		// Let underlying graphics system finish rendering this frame.
		auto time1 = Timer::getTime();
		rasterizer->frameEnd(time);
		auto time2 = Timer::getTime();
		auto current = narrow_cast<float>(time2 - time1);
		const float ALPHA = 0.2f;
		finishFrameDuration = finishFrameDuration * (1 - ALPHA) +
		                      current * ALPHA;

		if (paintFrame) {
			lastPaintTime = time2;
		}
	}
	if (motherboard.isActive() && !motherboard.isFastForwarding()) {
		eventDistributor.distributeEvent(FinishFrameEvent(
			rasterizer->getPostProcessor()->getVideoSource(),
			videoSourceSetting.getSource(),
			!paintFrame));
	}
}

void SMSVDPPixelRenderer::reInit()
{
	// Don't draw before frameStart() is called. This for example can happen
	// after a loadstate in the middle of a frame.
	frameSkipCounter = 999.0f; // force drawing of the next frame
	renderFrame = false;
	paintFrame = false;
}

void SMSVDPPixelRenderer::drawLine(
	unsigned y, std::span<const uint8_t, 320> paletteIndices)
{
	rasterizer->drawLine(y, paletteIndices);
}

void SMSVDPPixelRenderer::update(const Setting& setting) noexcept
{
	assert(&setting == one_of(&renderSettings.getMinFrameSkipSetting(),
	                          &renderSettings.getMaxFrameSkipSetting()));
	(void)setting;
	// Force drawing of frame.
	frameSkipCounter = 999.0f;
}

} // namespace openmsx
