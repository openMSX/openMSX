// $Id$

#include "V9990SDLRasterizer.hh"
#include "V9990.hh"
#include "RawFrame.hh"
#include "OutputSurface.hh"
#include "PostProcessor.hh"
#include "BooleanSetting.hh"
#include "Display.hh"
#include "VisibleSurface.hh"
#include "RenderSettings.hh"
#include "MemoryOps.hh"
#include "MSXMotherBoard.hh"
#include <algorithm>
#include <SDL.h>

using std::min;
using std::max;

namespace openmsx {

template <class Pixel>
V9990SDLRasterizer<Pixel>::V9990SDLRasterizer(
		V9990& vdp_, Display& display, VisibleSurface& screen_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, postProcessor(new PostProcessor<Pixel>(
		vdp.getMotherBoard().getCommandController(),
		display, screen_, VIDEO_GFX9000, 1280, 240))
	, bitmapConverter(vdp, palette64, palette256, palette32768)
	, p1Converter(vdp, palette64)
	, p2Converter(vdp, palette64)
	, deinterlaceSetting(display.getRenderSettings().getDeinterlace())
{
	workFrame = new RawFrame(screen.getFormat(), sizeof(Pixel), 1280, 240);

	// Fill palettes
	precalcPalettes();
}

template <class Pixel>
V9990SDLRasterizer<Pixel>::~V9990SDLRasterizer()
{
	delete workFrame;
}

template <class Pixel>
bool V9990SDLRasterizer<Pixel>::isActive()
{
	return postProcessor->getZ() != Layer::Z_MSX_PASSIVE;
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::reset()
{
	setDisplayMode(vdp.getDisplayMode());
	setColorMode(vdp.getColorMode());
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::frameStart()
{
	const V9990DisplayPeriod& horTiming = vdp.getHorizontalTiming();
	const V9990DisplayPeriod& verTiming = vdp.getVerticalTiming();

	// Center image on the window.

	// In SDLLo, one window pixel represents 8 UC clockticks, so the
	// window = 320 * 8 UC ticks wide. In SDLHi, one pixel is 4 clock-
	// ticks and the window 640 pixels wide -- same amount of UC ticks.
	colZero = horTiming.blank + horTiming.border1 +
	          (horTiming.display - SCREEN_WIDTH * 8) / 2;

	// 240 display lines can be shown. In SDLHi, we can do interlace,
	// but still 240 lines per frame.
	lineRenderTop = verTiming.blank + verTiming.border1 +
	                (verTiming.display - SCREEN_HEIGHT) / 2;
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::frameEnd()
{
	workFrame = postProcessor->rotateFrames(
	    workFrame,
	    vdp.isEvenOddEnabled() ? (vdp.getEvenOdd() ? RawFrame::FIELD_EVEN
	                                               : RawFrame::FIELD_ODD)
	                           : RawFrame::FIELD_NONINTERLACED);
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::setDisplayMode(V9990DisplayMode mode)
{
	displayMode = mode;
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::setColorMode(V9990ColorMode mode)
{
	colorMode = mode;
	bitmapConverter.setColorMode(mode);
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	Pixel bgColor = vdp.isOverScan()
	              ? 0
	              : palette64[vdp.getBackDropColor() & 63];

	int startY = max(fromY  - lineRenderTop,   0);
	int endY   = min(limitY - lineRenderTop, 240);
	if (startY >= endY) return;

	if ((fromX == 0) && (limitX == V9990DisplayTiming::UC_TICKS_PER_LINE)) {
		// optimization
		for (int y = startY; y < endY; ++y) {
			workFrame->setBlank(y, bgColor);
		}
		return;
	}

	static int const screenW = SCREEN_WIDTH * 8; // in ticks
	int startX = max(0, V9990::UCtoX(fromX - colZero, displayMode));
	int endX = V9990::UCtoX(
		(limitX == V9990DisplayTiming::UC_TICKS_PER_LINE)
		? screenW : min(screenW, limitX - colZero), displayMode);
	if (startX >= endX) return;

	unsigned lineWidth = vdp.getLineWidth();
	for (int y = startY; y < endY; ++y) {
		MemoryOps::memset<Pixel, MemoryOps::NO_STREAMING>(
			workFrame->getLinePtr(y, (Pixel*)0) + startX,
			endX - startX, bgColor);
		workFrame->setLineWidth(y, lineWidth);
	}
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY, int displayWidth, int displayHeight)
{
	static int const screenW = SCREEN_WIDTH * 8;
	static int const screenH = SCREEN_HEIGHT;

	if ((displayWidth > 0) && (displayHeight > 0)) {
		// from VDP coordinates to screen coordinates
		fromX -= colZero;
		fromY -= lineRenderTop;

		// Clip to screen
		if (fromX < 0) {
			displayX -= fromX;
			displayWidth += fromX;
			fromX = 0;
		}
		if ((fromX + displayWidth) > screenW) {
			displayWidth = screenW - fromX;
		}
		if (fromY < 0) {
			displayY -= fromY;
			displayHeight += fromY;
			fromY = 0;
		}
		if ((fromY + displayHeight) > screenH) {
			displayHeight = screenH - fromY;
		}

		if (displayHeight > 0) {
			fromX = V9990::UCtoX(fromX, displayMode);
			displayX = V9990::UCtoX(displayX, displayMode);
			displayWidth = V9990::UCtoX(displayWidth, displayMode);

			if (displayMode == P1) {
				drawP1Mode(fromX, fromY, displayX, displayY,
				           displayWidth, displayHeight);
			} else if (displayMode == P2) {
				drawP2Mode(fromX, fromY, displayX, displayY,
				           displayWidth, displayHeight);
			} else {
				drawBxMode(fromX, fromY, displayX, displayY,
				           displayWidth, displayHeight);
			}
		}
	}
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::drawP1Mode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	while (displayHeight--) {
		Pixel* pixelPtr = workFrame->getLinePtr(fromY, (Pixel*)0) + fromX;
		p1Converter.convertLine(pixelPtr, displayX, displayWidth,
		                        displayY);
		workFrame->setLineWidth(fromY, 320);
		++fromY;
		++displayY;
	}
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::drawP2Mode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	while (displayHeight--) {
		Pixel* pixelPtr = workFrame->getLinePtr(fromY, (Pixel*)0) + fromX;
		p2Converter.convertLine(pixelPtr, displayX, displayWidth,
		                        displayY);
		workFrame->setLineWidth(fromY, 640);
		++fromY;
		++displayY;
	}
}

static unsigned rollMasks[4] = {
	0x1FFF, // no rolling
	0x00FF,
	0x01FF,
	0x00FF // TODO check this (undocumented)
};

template <class Pixel>
void V9990SDLRasterizer<Pixel>::drawBxMode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	unsigned scrollX = vdp.getScrollAX();
	unsigned x = displayX + scrollX;

	unsigned scrollY = vdp.getScrollAY();
	int lineStep = 1;
	if (vdp.isEvenOddEnabled()) {
		if (vdp.getEvenOdd()) {
			++displayY;
		}
		lineStep = 2;
	}

	unsigned rollMask = rollMasks[scrollY >> 14];
	unsigned scrollYBase = scrollY & ~rollMask;
	while (displayHeight--) {
		unsigned y = scrollYBase + (displayY + scrollY) & rollMask;
		unsigned address = vdp.XYtoVRAM(&x, y, colorMode);
		Pixel* pixelPtr = workFrame->getLinePtr(fromY, (Pixel*)0) + fromX;
		bitmapConverter.convertLine(pixelPtr, address, displayWidth,
		                            displayY);
		workFrame->setLineWidth(fromY, vdp.getLineWidth());
		++fromY;
		displayY += lineStep;
	}
}


template <class Pixel>
void V9990SDLRasterizer<Pixel>::precalcPalettes()
{
	// the 32768 color palette
	for (int g = 0; g < 32; ++g) {
		for (int r = 0; r < 32; ++r) {
			for (int b = 0; b < 32; ++b) {
				palette32768[(g << 10) + (r << 5) + b] =
					SDL_MapRGB(screen.getFormat(),
					           (int)(r * (255.0 / 31.0)),
					           (int)(g * (255.0 / 31.0)),
					           (int)(b * (255.0 / 31.0)));
			}
		}
	}

	// the 256 color palette
	int mapRG[8] = { 0, 4, 9, 13, 18, 22, 27, 31 };
	int mapB [4] = { 0, 11, 21, 31 };
	for (int g = 0; g < 8; ++g) {
		for (int r = 0; r < 8; ++r) {
			for (int b = 0; b < 4; ++b) {
				palette256[(g << 5) + (r << 2) + b] =
					palette32768[(mapRG[g] << 10) +
					             (mapRG[r] <<  5) +
					              mapB [b]];
			}
		}
	}

	// get 64 color palette from VDP
	for (int i = 0; i < 64; ++i) {
		byte r, g, b;
		vdp.getPalette(i, r, g, b);
		setPalette(i, r, g, b);
	}
}

template <class Pixel>
void V9990SDLRasterizer<Pixel>::setPalette(int index,
                                                 byte r, byte g, byte b)
{
	palette64[index & 63] = palette32768[((g & 31) << 10) +
	                                     ((r & 31) <<  5) +
	                                      (b & 31)];
}


// Force template instantiation.
template class V9990SDLRasterizer<Uint16>;
template class V9990SDLRasterizer<Uint32>;

} // namespace openmsx

