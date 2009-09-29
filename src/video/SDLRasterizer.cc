// $Id$

#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RawFrame.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "Display.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "FloatSetting.hh"
#include "StringSetting.hh"
#include "MemoryOps.hh"
#include "VisibleSurface.hh"
#include "build-info.hh"
#include "components.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

/** VDP ticks between start of line and start of left border.
  */
static const int TICKS_LEFT_BORDER = 100 + 102;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to a central location?
  */
static const int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

template <class Pixel>
inline int SDLRasterizer<Pixel>::translateX(int absoluteX, bool narrow)
{
	int maxX = narrow ? 640 : 320;
	if (absoluteX == VDP::TICKS_PER_LINE) return maxX;

	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	const int ROUND_MASK = narrow ? ~1 : ~3;
	int screenX =
		((absoluteX & ROUND_MASK) - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		/ (narrow ? 2 : 4)
		+ maxX / 2;
	return std::max(screenX, 0);
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderBitmapLine(Pixel* buf, unsigned vramLine)
{
	if (vdp.getDisplayMode().isPlanar()) {
		const byte* vramPtr0;
		const byte* vramPtr1;
		vram.bitmapCacheWindow.getReadAreaPlanar(
			vramLine * 256, 256, vramPtr0, vramPtr1);
		bitmapConverter->convertLinePlanar(buf, vramPtr0, vramPtr1);
	} else {
		const byte* vramPtr =
			vram.bitmapCacheWindow.getReadArea(vramLine * 128, 128);
		bitmapConverter->convertLine(buf, vramPtr);
	}
}

template <class Pixel>
SDLRasterizer<Pixel>::SDLRasterizer(
		VDP& vdp_, Display& display, VisibleSurface& screen_,
		std::auto_ptr<PostProcessor> postProcessor_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, postProcessor(postProcessor_)
	, renderSettings(display.getRenderSettings())
	, characterConverter(new CharacterConverter<Pixel>(vdp, palFg, palBg))
	, bitmapConverter(new BitmapConverter<Pixel>(
	                                    palFg, PALETTE256, V9958_COLORS))
	, spriteConverter(new SpriteConverter<Pixel>(vdp.getSpriteChecker()))
{
	workFrame = new RawFrame(screen.getSDLFormat(), 640, 240);

	// Init the palette.
	precalcPalette();

	// Initialize palette (avoid UMR)
	if (!vdp.isMSX1VDP()) {
		for (int i = 0; i < 16; ++i) {
			palFg[i] = palFg[i + 16] = palBg[i] =
				V9938_COLORS[0][0][0];
		}
	}

	renderSettings.getGamma()      .attach(*this);
	renderSettings.getBrightness() .attach(*this);
	renderSettings.getContrast()   .attach(*this);
	renderSettings.getColorMatrix().attach(*this);
}

template <class Pixel>
SDLRasterizer<Pixel>::~SDLRasterizer()
{
	renderSettings.getColorMatrix().detach(*this);
	renderSettings.getGamma()      .detach(*this);
	renderSettings.getBrightness() .detach(*this);
	renderSettings.getContrast()   .detach(*this);

	delete workFrame;
}

template <class Pixel>
bool SDLRasterizer<Pixel>::isActive()
{
	return postProcessor->getZ() != Layer::Z_MSX_PASSIVE;
}

template <class Pixel>
void SDLRasterizer<Pixel>::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter->setTransparency(vdp.getTransparency());
	setSuperimposing(vdp.isSuperimposing());

	resetPalette();
}

template <class Pixel>
void SDLRasterizer<Pixel>::resetPalette()
{
	if (!vdp.isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp.getPalette(i));
		}
	}
}

template<class Pixel>
void SDLRasterizer<Pixel>::setSuperimposing(bool enabled)
{
	postProcessor->setTransparency(enabled);
	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                    enabled, vdp.getBackgroundColor());
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(workFrame,
	    vdp.isInterlaced()
	    ? (vdp.getEvenOdd() ? FrameSource::FIELD_ODD : FrameSource::FIELD_EVEN)
	    : FrameSource::FIELD_NONINTERLACED,
	    time);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameEnd()
{
	// Nothing to do.
}

template <class Pixel>
void SDLRasterizer<Pixel>::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter->setDisplayMode(mode);
	} else {
		characterConverter->setDisplayMode(mode);
	}
	precalcColorIndex0(mode, vdp.getTransparency(),
	                    vdp.isSuperimposing(), vdp.getBackgroundColor());
	spriteConverter->setDisplayMode(mode);
	spriteConverter->setPalette(mode.getByte() == DisplayMode::GRAPHIC7
	                            ? palGraphic7Sprites : palBg);
}

template <class Pixel>
void SDLRasterizer<Pixel>::setPalette(int index, int grb)
{
	// Update SDL colors in palette.
	Pixel newColor = V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
	palFg[index     ] = newColor;
	palFg[index + 16] = newColor;
	palBg[index     ] = newColor;
	bitmapConverter->palette16Changed();

	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                    vdp.isSuperimposing(), vdp.getBackgroundColor());
}

template <class Pixel>
void SDLRasterizer<Pixel>::setBackgroundColor(int index)
{
	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                    vdp.isSuperimposing(), index);
}

template <class Pixel>
void SDLRasterizer<Pixel>::setTransparency(bool enabled)
{
	spriteConverter->setTransparency(enabled);
	precalcColorIndex0(vdp.getDisplayMode(), enabled,
	                    vdp.isSuperimposing(), vdp.getBackgroundColor());
}

template<typename Pixel>
Pixel SDLRasterizer<Pixel>::calcColorHelper(double r, double g, double b)
{
	renderSettings.transformRGB(r, g, b);
	Pixel p = screen.mapRGB(r, g, b);
	if (sizeof(Pixel) == 2) {
		return (p != screen.getKeyColor<Pixel>())
		      ? p
		      : screen.getKeyColorClash<Pixel>();
	} else {
		assert(p != screen.getKeyColor<Pixel>());
		return p;
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcPalette()
{
	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte* rgb = Renderer::TMS99X8A_PALETTE[i];
			double dr = rgb[0] / 255.0;
			double dg = rgb[1] / 255.0;
			double db = rgb[2] / 255.0;
			palFg[i] = palFg[i + 16] = palBg[i] =
				calcColorHelper(dr, dg, db);
		}
	} else {
		if (vdp.hasYJK()) {
			// Precalculate palette for V9958 colors.
			for (int r = 0; r < 32; r++) {
				for (int g = 0; g < 32; g++) {
					for (int b = 0; b < 32; b++) {
						double dr = r / 31.0;
						double dg = g / 31.0;
						double db = b / 31.0;
						V9958_COLORS[(r<<10) + (g<<5) + b] =
							calcColorHelper(dr, dg, db);
					}
				}
			}
			// Precalculate palette for V9938 colors.
			// Based on comparing red and green gradients, using palette and
			// YJK, in SCREEN11 on a real turbo R.
			for (int r = 0; r < 8; r++) {
				int r5 = (r << 2) | (r >> 1);
				for (int g = 0; g < 8; g++) {
					int g5 = (g << 2) | (g >> 1);
					for (int b = 0; b < 8; b++) {
						int b5 = (b << 2) | (b >> 1);
						V9938_COLORS[r][g][b] =
							V9958_COLORS[(r5<<10) + (g5<<5) + b5];
					}
				}
			}
		} else {
			// Precalculate palette for V9938 colors.
			for (int r = 0; r < 8; r++) {
				for (int g = 0; g < 8; g++) {
					for (int b = 0; b < 8; b++) {
						double dr = r / 7.0;
						double dg = g / 7.0;
						double db = b / 7.0;
						V9938_COLORS[r][g][b] =
							calcColorHelper(dr, dg, db);
					}
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (int i = 0; i < 256; i++) {
			PALETTE256[i] = V9938_COLORS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[(i & 0x03) == 3 ? 7 : (i & 0x03) * 2];
		}
		// Precalculate Graphic 7 sprite palette.
		for (int i = 0; i < 16; i++) {
			word grb = Renderer::GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcColorIndex0(DisplayMode mode,
		bool transparency, bool superimposing, byte bgcolorIndex)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}

	int tpIndex = transparency ? bgcolorIndex : 0;
	if (mode.getBase() != DisplayMode::GRAPHIC5) {
		Pixel c = (superimposing && bgcolorIndex == 0)
		        ? screen.getKeyColor<Pixel>()
		        : palBg[tpIndex];

		if (palFg[0] != c) {
			palFg[0] = c;
			bitmapConverter->palette16Changed();
		}
	} else {
		// TODO: superimposing
		if ((palFg[ 0] != palBg[tpIndex >> 2]) ||
		    (palFg[16] != palBg[tpIndex &  3])) {
			palFg[ 0] = palBg[tpIndex >> 2];
			palFg[16] = palBg[tpIndex &  3];
			bitmapConverter->palette16Changed();
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	DisplayMode mode = vdp.getDisplayMode();
	byte modeBase = mode.getBase();
	int bgColor = vdp.getBackgroundColor();
	Pixel border0, border1;
	if (modeBase == DisplayMode::GRAPHIC5) {
		// border in SCREEN6 has separate color for even and odd pixels.
		// TODO odd/even swapped?
		border0 = palBg[(bgColor & 0x0C) >> 2];
		border1 = palBg[(bgColor & 0x03) >> 0];
	} else if (modeBase == DisplayMode::GRAPHIC7) {
		border0 = border1 = PALETTE256[bgColor];
	} else {
		if (!bgColor && vdp.isSuperimposing()) {
			border0 = border1 = screen.getKeyColor<Pixel>();
		} else {
			border0 = border1 = palBg[bgColor];
		}
	}

	int startY = std::max(fromY - lineRenderTop, 0);
	int endY = std::min(limitY - lineRenderTop, 240);
	if ((fromX == 0) && (limitX == VDP::TICKS_PER_LINE) &&
	    (border0 == border1)) {
		// complete lines, non striped
		for (int y = startY; y < endY; y++) {
			workFrame->setBlank(y, border0);
		}
	} else {
		unsigned lineWidth = mode.getLineWidth();
		unsigned x = translateX(fromX, (lineWidth == 512));
		unsigned num = translateX(limitX, (lineWidth == 512)) - x;
		unsigned width = (lineWidth == 512) ? 640 : 320;
		MemoryOps::MemSet2<Pixel, MemoryOps::NO_STREAMING> memset;
		for (int y = startY; y < endY; ++y) {
			memset(workFrame->getLinePtrDirect<Pixel>(y) + x,
			       num, border0, border1);
			workFrame->setLineWidth(y, width);
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	DisplayMode mode = vdp.getDisplayMode();
	unsigned lineWidth = mode.getLineWidth();
	if (lineWidth == 256) {
		int endX = displayX + displayWidth;
		displayX /= 2;
		displayWidth = endX / 2 - displayX;
	}

	// Clip to screen area.
	int screenLimitY = std::min(
		fromY + displayHeight - lineRenderTop,
		240);
	int screenY = fromY - lineRenderTop;
	if (screenY < 0) {
		displayY -= screenY;
		fromY = lineRenderTop;
		screenY = 0;
	}
	displayHeight = screenLimitY - screenY;
	if (displayHeight <= 0) return;

	int leftBackground =
		translateX(vdp.getLeftBackground(), lineWidth == 512);
	// TODO: Find out why this causes 1-pixel jitter:
	//dest.x = translateX(fromX);
	int hScroll =
		  mode.isTextMode()
		? 0
		: 8 * (lineWidth / 256) * (vdp.getHorizontalScrollHigh() & 0x1F);

	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	int pageBorder = displayX + displayWidth;
	int scrollPage1, scrollPage2;
	if (vdp.isMultiPageScrolling()) {
		scrollPage1 = vdp.getHorizontalScrollHigh() >> 5;
		scrollPage2 = scrollPage1 ^ 1;
	} else {
		scrollPage1 = 0;
		scrollPage2 = 0;
	}
	// Because SDL blits do not wrap, unlike GL textures, the pageBorder is
	// also used if multi page is disabled.
	int pageSplit = lineWidth - hScroll;
	if (pageSplit < pageBorder) {
		pageBorder = pageSplit;
	}

	if (mode.isBitmapMode()) {
		// Which bits in the name mask determine the page?
		int pageMaskOdd = (mode.isPlanar() ? 0x000 : 0x200) |
		                  vdp.getEvenOddMask();
		int pageMaskEven = vdp.isMultiPageScrolling()
		                 ? (pageMaskOdd & ~0x100)
		                 : pageMaskOdd;

		for (int y = screenY; y < screenLimitY; y++) {
			const int vramLine[2] = {
				(vram.nameTable.getMask() >> 7) & (pageMaskEven | displayY),
				(vram.nameTable.getMask() >> 7) & (pageMaskOdd  | displayY)
			};

			Pixel buf[512];
			int lineInBuf = -1; // buffer data not valid
			Pixel* dst = workFrame->getLinePtrDirect<Pixel>(y)
			           + leftBackground + displayX;
			int firstPageWidth = pageBorder - displayX;
			if (firstPageWidth > 0) {
				if ((displayX + hScroll) == 0) {
					renderBitmapLine(dst, vramLine[scrollPage1]);
				} else {
					lineInBuf = vramLine[scrollPage1];
					renderBitmapLine(buf, vramLine[scrollPage1]);
					const Pixel* src = buf + displayX + hScroll;
					MemoryOps::stream_memcpy(dst, src, firstPageWidth);
				}
			} else {
				firstPageWidth = 0;
			}
			if (firstPageWidth < displayWidth) {
				if (lineInBuf != vramLine[scrollPage2]) {
					renderBitmapLine(buf, vramLine[scrollPage2]);
				}
				unsigned x = displayX < pageBorder
					   ? 0 : displayX + hScroll - lineWidth;
				MemoryOps::stream_memcpy(
					dst + firstPageWidth,
					buf + x,
					displayWidth - firstPageWidth);
			}
			workFrame->setLineWidth(y, (lineWidth == 512) ? 640 : 320);

			displayY = (displayY + 1) & 255;
		}
	} else {
		// horizontal scroll (high) is implemented in CharacterConverter
		for (int y = screenY; y < screenLimitY; y++) {
			assert(!vdp.isMSX1VDP() || displayY < 192);

			Pixel* dst = workFrame->getLinePtrDirect<Pixel>(y)
			           + leftBackground + displayX;
			if (displayX == 0) {
				characterConverter->convertLine(dst, displayY);
			} else {
				Pixel buf[512];
				characterConverter->convertLine(buf, displayY);
				const Pixel* src = buf + displayX;
				MemoryOps::stream_memcpy(dst, src, displayWidth);
			}

			workFrame->setLineWidth(y, (lineWidth == 512) ? 640 : 320);
			displayY = (displayY + 1) & 255;
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawSprites(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	// Clip to screen area.
	// TODO: Code duplicated from drawDisplay.
	int screenLimitY = std::min(
		fromY + displayHeight - lineRenderTop,
		240);
	int screenY = fromY - lineRenderTop;
	if (screenY < 0) {
		displayY -= screenY;
		fromY = lineRenderTop;
		screenY = 0;
	}
	displayHeight = screenLimitY - screenY;
	if (displayHeight <= 0) return;

	// Render sprites.
	// TODO: Call different SpriteConverter methods depending on narrow/wide
	//       pixels in this display mode?
	int spriteMode = vdp.getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	int screenX = translateX(
		vdp.getLeftSprites(),
		vdp.getDisplayMode().getLineWidth() == 512);
	if (spriteMode == 1) {
		for (int y = fromY; y < limitY; y++, screenY++) {
			Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
			spriteConverter->drawMode1(y, displayX, displayLimitX, pixelPtr);
		}
	} else {
		byte mode = vdp.getDisplayMode().getByte();
		if (mode == DisplayMode::GRAPHIC5) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter->template drawMode2<DisplayMode::GRAPHIC5>(
					y, displayX, displayLimitX, pixelPtr);
			}
		} else if (mode == DisplayMode::GRAPHIC6) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter->template drawMode2<DisplayMode::GRAPHIC6>(
					y, displayX, displayLimitX, pixelPtr);
			}
		} else {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter->template drawMode2<DisplayMode::GRAPHIC4>(
					y, displayX, displayLimitX, pixelPtr);
			}
		}
	}
}

template <class Pixel>
bool SDLRasterizer<Pixel>::isRecording() const
{
	return postProcessor->isRecording();
}

template <class Pixel>
void SDLRasterizer<Pixel>::update(const Setting& setting)
{
	if ((&setting == &renderSettings.getGamma()) ||
	    (&setting == &renderSettings.getBrightness()) ||
	    (&setting == &renderSettings.getContrast()) ||
	    (&setting == &renderSettings.getColorMatrix())) {
		precalcPalette();
		resetPalette();
	}
}


// Force template instantiation.
#if HAVE_16BPP
template class SDLRasterizer<word>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class SDLRasterizer<unsigned>;
#endif

} // namespace openmsx
