#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RawFrame.hh"
#include "Display.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "MemoryOps.hh"
#include "VisibleSurface.hh"
#include "build-info.hh"
#include "components.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>

using namespace gl;

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

/** Translate from absolute VDP coordinates to screen coordinates:
  * Note: In reality, there are only 569.5 visible pixels on a line.
  *       Because it looks better, the borders are extended to 640.
  * @param absoluteX Absolute VDP coordinate.
  * @param narrow Is this a narrow (512 pixels wide) display mode?
  */
static inline int translateX(int absoluteX, bool narrow)
{
	int maxX = narrow ? 640 : 320;
	if (absoluteX == VDP::TICKS_PER_LINE) return maxX;

	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	const int ROUND_MASK = narrow ? ~1 : ~3;
	int screenX =
		((absoluteX - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		>> (narrow ? 1 : 2))
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
		bitmapConverter.convertLinePlanar(buf, vramPtr0, vramPtr1);
	} else {
		const byte* vramPtr =
			vram.bitmapCacheWindow.getReadArea(vramLine * 128, 128);
		bitmapConverter.convertLine(buf, vramPtr);
	}
}

template <class Pixel>
SDLRasterizer<Pixel>::SDLRasterizer(
		VDP& vdp_, Display& display, VisibleSurface& screen_,
		std::unique_ptr<PostProcessor> postProcessor_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(screen.getSDLFormat(), 640, 240))
	, renderSettings(display.getRenderSettings())
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLORS)
	, spriteConverter(vdp.getSpriteChecker())
{
	// Init the palette.
	precalcPalette();

	// Initialize palette (avoid UMR)
	if (!vdp.isMSX1VDP()) {
		for (int i = 0; i < 16; ++i) {
			palFg[i] = palFg[i + 16] = palBg[i] =
				V9938_COLORS[0][0][0];
		}
	}

	renderSettings.getGammaSetting()      .attach(*this);
	renderSettings.getBrightnessSetting() .attach(*this);
	renderSettings.getContrastSetting()   .attach(*this);
	renderSettings.getColorMatrixSetting().attach(*this);
}

template <class Pixel>
SDLRasterizer<Pixel>::~SDLRasterizer()
{
	renderSettings.getColorMatrixSetting().detach(*this);
	renderSettings.getGammaSetting()      .detach(*this);
	renderSettings.getBrightnessSetting() .detach(*this);
	renderSettings.getContrastSetting()   .detach(*this);
}

template <class Pixel>
PostProcessor* SDLRasterizer<Pixel>::getPostProcessor() const
{
	return postProcessor.get();
}

template <class Pixel>
bool SDLRasterizer<Pixel>::isActive()
{
	return postProcessor->needRender() &&
	       vdp.getMotherBoard().isActive() &&
	       !vdp.getMotherBoard().isFastForwarding();
}

template <class Pixel>
void SDLRasterizer<Pixel>::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter.setTransparency(vdp.getTransparency());

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
void SDLRasterizer<Pixel>::setSuperimposeVideoFrame(const RawFrame* videoSource)
{
	postProcessor->setSuperimposeVideoFrame(videoSource);
	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                   videoSource, vdp.getBackgroundColor());
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
	workFrame->init(
	    vdp.isInterlaced() ? (vdp.getEvenOdd() ? FrameSource::FIELD_ODD
	                                           : FrameSource::FIELD_EVEN)
	                       : FrameSource::FIELD_NONINTERLACED);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;


	// We haven't drawn any left/right borders yet this frame, thus so far
	// all is still consistent (same settings for all left/right borders).
	mixedLeftRightBorders = false;

	auto& borderInfo = workFrame->getBorderInfo();
	Pixel color0, color1;
	getBorderColors(color0, color1);
	canSkipLeftRightBorders =
		(borderInfo.mode   == vdp.getDisplayMode().getByte()) &&
		(borderInfo.color0 == color0)                         &&
		(borderInfo.color1 == color1)                         &&
		(borderInfo.adjust == vdp.getHorizontalAdjust())      &&
		(borderInfo.scroll == vdp.getHorizontalScrollLow())   &&
		(borderInfo.masked == vdp.isBorderMasked());
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameEnd()
{
	auto& borderInfo = workFrame->getBorderInfo();
	if (mixedLeftRightBorders) {
		// This frame contains left/right borders drawn with different
		// settings. So don't use it as a starting point for future
		// border drawing optimizations.
		borderInfo.mode = 0xff; // invalid mode, other fields don't matter
	} else {
		// All left/right borders in this frame are uniform (drawn with
		// the same settings). If in a later frame the border-related
		// settings are still the same, we can skip drawing borders.
		Pixel color0, color1;
		getBorderColors(color0, color1);
		borderInfo.mode   = vdp.getDisplayMode().getByte();
		borderInfo.color0 = color0;
		borderInfo.color1 = color1;
		borderInfo.adjust = vdp.getHorizontalAdjust();
		borderInfo.scroll = vdp.getHorizontalScrollLow();
		borderInfo.masked = vdp.isBorderMasked();
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::borderSettingChanged()
{
	// Can no longer use the skip-border drawing optimization this frame.
	canSkipLeftRightBorders = false;

	// Cannot use this frame as a starting point for future skip-border
	// optimizations.
	mixedLeftRightBorders = true;
}

template <class Pixel>
void SDLRasterizer<Pixel>::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	precalcColorIndex0(mode, vdp.getTransparency(),
	                   vdp.isSuperimposing(), vdp.getBackgroundColor());
	spriteConverter.setDisplayMode(mode);
	spriteConverter.setPalette(mode.getByte() == DisplayMode::GRAPHIC7
	                           ? palGraphic7Sprites : palBg);

	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setPalette(int index, int grb)
{
	// Update SDL colors in palette.
	Pixel newColor = V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
	palFg[index     ] = newColor;
	palFg[index + 16] = newColor;
	palBg[index     ] = newColor;
	bitmapConverter.palette16Changed();

	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                   vdp.isSuperimposing(), vdp.getBackgroundColor());
	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setBackgroundColor(int index)
{
	if (vdp.getDisplayMode().getByte() != DisplayMode::GRAPHIC7) {
		precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
				   vdp.isSuperimposing(), index);
	}
	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setHorizontalAdjust(int /*adjust*/)
{
	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setHorizontalScrollLow(byte /*scroll*/)
{
	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setBorderMask(bool /*masked*/)
{
	borderSettingChanged();
}

template <class Pixel>
void SDLRasterizer<Pixel>::setTransparency(bool enabled)
{
	spriteConverter.setTransparency(enabled);
	precalcColorIndex0(vdp.getDisplayMode(), enabled,
	                   vdp.isSuperimposing(), vdp.getBackgroundColor());
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcPalette()
{
	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		const auto palette = vdp.getMSX1Palette();
		for (int i = 0; i < 16; ++i) {
			const auto rgb = palette[i];
			palFg[i] = palFg[i + 16] = palBg[i] =
				screen.mapKeyedRGB<Pixel>(
					renderSettings.transformRGB(
						vec3(rgb[0], rgb[1], rgb[2]) / 255.0f));
		}
	} else {
		if (vdp.hasYJK()) {
			// Precalculate palette for V9958 colors.
			if (renderSettings.isColorMatrixIdentity()) {
				// Most users use the "normal" monitor type; making this a
				// special case speeds up palette precalculation a lot.
				int intensity[32];
				for (int i = 0; i < 32; ++i) {
					intensity[i] =
						int(255 * renderSettings.transformComponent(i / 31.0));
				}
				for (int rgb = 0; rgb < (1 << 15); ++rgb) {
					V9958_COLORS[rgb] = screen.mapKeyedRGB255<Pixel>(ivec3(
						intensity[(rgb >> 10) & 31],
						intensity[(rgb >>  5) & 31],
						intensity[(rgb >>  0) & 31]));
				}
			} else {
				for (int r = 0; r < 32; ++r) {
					for (int g = 0; g < 32; ++g) {
						for (int b = 0; b < 32; ++b) {
							V9958_COLORS[(r << 10) + (g << 5) + b] =
								screen.mapKeyedRGB<Pixel>(
									renderSettings.transformRGB(
										vec3(r, g, b) / 31.0f));
						}
					}
				}
			}
			// Precalculate palette for V9938 colors.
			// Based on comparing red and green gradients, using palette and
			// YJK, in SCREEN11 on a real turbo R.
			for (int r3 = 0; r3 < 8; ++r3) {
				int r5 = (r3 << 2) | (r3 >> 1);
				for (int g3 = 0; g3 < 8; ++g3) {
					int g5 = (g3 << 2) | (g3 >> 1);
					for (int b3 = 0; b3 < 8; ++b3) {
						int b5 = (b3 << 2) | (b3 >> 1);
						V9938_COLORS[r3][g3][b3] =
							V9958_COLORS[(r5 << 10) + (g5 << 5) + b5];
					}
				}
			}
		} else {
			// Precalculate palette for V9938 colors.
			if (renderSettings.isColorMatrixIdentity()) {
				int intensity[8];
				for (int i = 0; i < 8; ++i) {
					intensity[i] =
						int(255 * renderSettings.transformComponent(i / 7.0f));
				}
				for (int r = 0; r < 8; ++r) {
					for (int g = 0; g < 8; ++g) {
						for (int b = 0; b < 8; ++b) {
							V9938_COLORS[r][g][b] =
								screen.mapKeyedRGB255<Pixel>(ivec3(
									intensity[r],
									intensity[g],
									intensity[b]));
						}
					}
				}
			} else {
				for (int r = 0; r < 8; ++r) {
					for (int g = 0; g < 8; ++g) {
						for (int b = 0; b < 8; ++b) {
							V9938_COLORS[r][g][b] =
								screen.mapKeyedRGB<Pixel>(
									renderSettings.transformRGB(
										vec3(r, g, b) / 7.0f));;
						}
					}
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (int i = 0; i < 256; ++i) {
			PALETTE256[i] = V9938_COLORS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[(i & 0x03) == 3 ? 7 : (i & 0x03) * 2];
		}
		// Precalculate Graphic 7 sprite palette.
		for (int i = 0; i < 16; ++i) {
			uint16_t grb = Renderer::GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcColorIndex0(DisplayMode mode,
		bool transparency, const RawFrame* superimposing, byte bgcolorIndex)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}

	int tpIndex = transparency ? bgcolorIndex : 0;
	if (mode.getBase() != DisplayMode::GRAPHIC5) {
		Pixel c = (superimposing && (bgcolorIndex == 0))
		        ? screen.getKeyColor<Pixel>()
		        : palBg[tpIndex];

		if (palFg[0] != c) {
			palFg[0] = c;
			bitmapConverter.palette16Changed();
		}
	} else {
		// TODO: superimposing
		if ((palFg[ 0] != palBg[tpIndex >> 2]) ||
		    (palFg[16] != palBg[tpIndex &  3])) {
			palFg[ 0] = palBg[tpIndex >> 2];
			palFg[16] = palBg[tpIndex &  3];
			bitmapConverter.palette16Changed();
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::getBorderColors(Pixel& border0, Pixel& border1)
{
	DisplayMode mode = vdp.getDisplayMode();
	int bgColor = vdp.getBackgroundColor();
	if (mode.getBase() == DisplayMode::GRAPHIC5) {
		// border in SCREEN6 has separate color for even and odd pixels.
		// TODO odd/even swapped?
		border0 = palBg[(bgColor & 0x0C) >> 2];
		border1 = palBg[(bgColor & 0x03) >> 0];
	} else if (mode.getByte() == DisplayMode::GRAPHIC7) {
		border0 = border1 = PALETTE256[bgColor];
	} else {
		if (!bgColor && vdp.isSuperimposing()) {
			border0 = border1 = screen.getKeyColor<Pixel>();
		} else {
			border0 = border1 = palBg[bgColor];
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	Pixel border0, border1;
	getBorderColors(border0, border1);

	int startY = std::max(fromY - lineRenderTop, 0);
	int endY = std::min(limitY - lineRenderTop, 240);
	if ((fromX == 0) && (limitX == VDP::TICKS_PER_LINE) &&
	    (border0 == border1)) {
		// complete lines, non striped
		for (int y = startY; y < endY; y++) {
			workFrame->setBlank(y, border0);
			// setBlank() implies this line is not suitable
			// for left/right border optimization in a later
			// frame.
		}
	} else {
		unsigned lineWidth = vdp.getDisplayMode().getLineWidth();
		unsigned x = translateX(fromX, (lineWidth == 512));
		unsigned num = translateX(limitX, (lineWidth == 512)) - x;
		unsigned width = (lineWidth == 512) ? 640 : 320;
		MemoryOps::MemSet2<Pixel> memset;
		for (int y = startY; y < endY; ++y) {
			// workFrame->linewidth != 1 means the line has
			// left/right borders.
			if (canSkipLeftRightBorders &&
			    (workFrame->getLineWidthDirect(y) != 1)) continue;
			memset(workFrame->getLinePtrDirect<Pixel>(y) + x,
			       num, border0, border1);
			if (limitX == VDP::TICKS_PER_LINE) {
				// Only set line width at the end (right
				// border) of the line. This ensures we can
				// keep testing the width of the previous
				// version of this line for all (partial)
				// updates of this line.
				workFrame->setLineWidth(y, width);
			}
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	// Note: we don't call workFrame->setLineWidth() because that's done in
	// drawBorder() (for the right border). And the value we set there is
	// anyway the same as the one we would set here.
	DisplayMode mode = vdp.getDisplayMode();
	int lineWidth = mode.getLineWidth();
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

		for (int y = screenY; y < screenLimitY; y++) {
			// Which bits in the name mask determine the page?
			int pageMaskOdd = (mode.isPlanar() ? 0x000 : 0x200) |
				vdp.getEvenOddMask(y);
			int pageMaskEven = vdp.isMultiPageScrolling()
				? (pageMaskOdd & ~0x100)
				: pageMaskOdd;
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
				if (((displayX + hScroll) == 0) &&
				    (firstPageWidth == lineWidth)) {
					// fast-path, directly render to destination
					renderBitmapLine(dst, vramLine[scrollPage1]);
				} else {
					lineInBuf = vramLine[scrollPage1];
					renderBitmapLine(buf, vramLine[scrollPage1]);
					const Pixel* src = buf + displayX + hScroll;
					memcpy(dst, src, firstPageWidth * sizeof(Pixel));
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
				memcpy(dst + firstPageWidth,
				       buf + x,
				       (displayWidth - firstPageWidth) * sizeof(Pixel));
			}

			displayY = (displayY + 1) & 255;
		}
	} else {
		// horizontal scroll (high) is implemented in CharacterConverter
		for (int y = screenY; y < screenLimitY; y++) {
			assert(!vdp.isMSX1VDP() || displayY < 192);

			Pixel* dst = workFrame->getLinePtrDirect<Pixel>(y)
			           + leftBackground + displayX;
			if ((displayX == 0) && (displayWidth == lineWidth)){
				characterConverter.convertLine(dst, displayY);
			} else {
				Pixel buf[512];
				characterConverter.convertLine(buf, displayY);
				const Pixel* src = buf + displayX;
				memcpy(dst, src, displayWidth * sizeof(Pixel));
			}

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
	int spriteMode = vdp.getDisplayMode().getSpriteMode(vdp.isMSX1VDP());
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	int screenX = translateX(
		vdp.getLeftSprites(),
		vdp.getDisplayMode().getLineWidth() == 512);
	if (spriteMode == 1) {
		for (int y = fromY; y < limitY; y++, screenY++) {
			Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
			spriteConverter.drawMode1(y, displayX, displayLimitX, pixelPtr);
		}
	} else {
		byte mode = vdp.getDisplayMode().getByte();
		if (mode == DisplayMode::GRAPHIC5) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC5>(
					y, displayX, displayLimitX, pixelPtr);
			}
		} else if (mode == DisplayMode::GRAPHIC6) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC6>(
					y, displayX, displayLimitX, pixelPtr);
			}
		} else {
			for (int y = fromY; y < limitY; y++, screenY++) {
				Pixel* pixelPtr = workFrame->getLinePtrDirect<Pixel>(screenY) + screenX;
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC4>(
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
	if ((&setting == &renderSettings.getGammaSetting()) ||
	    (&setting == &renderSettings.getBrightnessSetting()) ||
	    (&setting == &renderSettings.getContrastSetting()) ||
	    (&setting == &renderSettings.getColorMatrixSetting())) {
		precalcPalette();
		resetPalette();
	}
}


// Force template instantiation.
#if HAVE_16BPP
template class SDLRasterizer<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class SDLRasterizer<uint32_t>;
#endif

} // namespace openmsx
