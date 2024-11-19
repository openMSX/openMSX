#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RawFrame.hh"
#include "Display.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "MemoryOps.hh"
#include "OutputSurface.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

using namespace gl;

namespace openmsx {

using Pixel = SDLRasterizer::Pixel;

/** VDP ticks between start of line and start of left border.
  */
static constexpr int TICKS_LEFT_BORDER = 100 + 102;
static constexpr int TICKS_RIGHT_BORDER = 100 + 102 + 56 + 1024 + 59;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to a central location?
  */
static constexpr int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

/** Translate from absolute VDP coordinates to screen coordinates:
  * Note: In reality, there are only 569.5 visible pixels on a line.
  *       Because it looks better, the borders are extended to 640.
  * @param absoluteX Absolute VDP coordinate.
  * @param narrow Is this a narrow (512 pixels wide) display mode?
  */
static constexpr int translateX(int absoluteX, bool narrow)
{
	int maxX = narrow ? 640 : 320;

	// Clip positions outside left/right VDP border to left/right "extended"
	// border ("extended" because we map 569.5 VDP pixels to 640 host pixels).
	// This prevents "inventing" new pixels that the real VDP would not
	// render (e.g. by rapidly changing the border-color register). So we
	// only extend actual VDP pixels.
	//
	// This was also needed to prevent a UMR:
	// * The VDP switches to a new display mode at a specific moment in the
	//   line, for details see VDP::syncAtNextLine(). Summary: (with
	//   set-adjust=0) that is 144 cycles past the (start of the) sync
	//   signal.
	// * Without this check, for 'absoluteX=144', this function would return
	//   a non-zero value.
	// * Suppose we're switching from a 256 to a 512 pixel wide mode. This
	//   switch happens at cycle=144, but that meant the first few pixels
	//   are still renderer at width=256 and the rest of the line at
	//   width=512. But, without this check, that meant some pixels in the
	//   640-pixel wide host buffer are never written, resulting in a UMR.
	// * Clipping to TICKS_{LEFT,RIGHT}_BORDER prevents this.
	if (absoluteX < TICKS_LEFT_BORDER) return 0;
	if (absoluteX > TICKS_RIGHT_BORDER) return maxX;

	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	const int ROUND_MASK = narrow ? ~1 : ~3;
	return ((absoluteX - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		>> (narrow ? 1 : 2))
		+ maxX / 2;
}

inline void SDLRasterizer::renderBitmapLine(std::span<Pixel> buf, unsigned vramLine)
{
	if (vdp.getDisplayMode().isPlanar()) {
		auto [vramPtr0, vramPtr1] =
			vram.bitmapCacheWindow.getReadAreaPlanar<256>(vramLine * 256);
		bitmapConverter.convertLinePlanar(buf, vramPtr0, vramPtr1);
	} else {
		auto vramPtr =
			vram.bitmapCacheWindow.getReadArea<128>(vramLine * 128);
		bitmapConverter.convertLine(buf, vramPtr);
	}
}

SDLRasterizer::SDLRasterizer(
		VDP& vdp_, Display& display, OutputSurface& screen_,
		std::unique_ptr<PostProcessor> postProcessor_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(640, 240))
	, renderSettings(display.getRenderSettings())
	, characterConverter(vdp, subspan<16>(palFg), palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLORS)
	, spriteConverter(vdp.getSpriteChecker(), palBg)
{
	// Init the palette.
	precalcPalette();

	// Initialize palette (avoid UMR)
	if (!vdp.isMSX1VDP()) {
		for (auto i : xrange(16)) {
			palFg[i] = palFg[i + 16] = palBg[i] =
				V9938_COLORS[0][0][0];
		}
	}

	renderSettings.getGammaSetting()      .attach(*this);
	renderSettings.getBrightnessSetting() .attach(*this);
	renderSettings.getContrastSetting()   .attach(*this);
	renderSettings.getColorMatrixSetting().attach(*this);
}

SDLRasterizer::~SDLRasterizer()
{
	renderSettings.getColorMatrixSetting().detach(*this);
	renderSettings.getGammaSetting()      .detach(*this);
	renderSettings.getBrightnessSetting() .detach(*this);
	renderSettings.getContrastSetting()   .detach(*this);
}

PostProcessor* SDLRasterizer::getPostProcessor() const
{
	return postProcessor.get();
}

bool SDLRasterizer::isActive()
{
	return postProcessor->needRender() &&
	       vdp.getMotherBoard().isActive() &&
	       !vdp.getMotherBoard().isFastForwarding();
}

void SDLRasterizer::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter.setTransparency(vdp.getTransparency());

	resetPalette();
}

void SDLRasterizer::resetPalette()
{
	if (!vdp.isMSX1VDP()) {
		// Reset the palette.
		for (auto i : xrange(16)) {
			setPalette(i, vdp.getPalette(i));
		}
	}
}

void SDLRasterizer::setSuperimposeVideoFrame(const RawFrame* videoSource)
{
	postProcessor->setSuperimposeVideoFrame(videoSource);
	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                   videoSource, vdp.getBackgroundColor());
}

void SDLRasterizer::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
	workFrame->init(
	    vdp.isInterlaced() ? (vdp.getEvenOdd() ? FrameSource::FieldType::ODD
	                                           : FrameSource::FieldType::EVEN)
	                       : FrameSource::FieldType::NONINTERLACED);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;
}

void SDLRasterizer::frameEnd()
{
}

void SDLRasterizer::setDisplayMode(DisplayMode mode)
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

}

void SDLRasterizer::setPalette(unsigned index, int grb)
{
	// Update SDL colors in palette.
	Pixel newColor = V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
	palFg[index     ] = newColor;
	palFg[index + 16] = newColor;
	palBg[index     ] = newColor;
	bitmapConverter.palette16Changed();

	precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                   vdp.isSuperimposing(), vdp.getBackgroundColor());
}

void SDLRasterizer::setBackgroundColor(byte index)
{
	if (vdp.getDisplayMode().getByte() != DisplayMode::GRAPHIC7) {
		precalcColorIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
				   vdp.isSuperimposing(), index);
	}
}

void SDLRasterizer::setHorizontalAdjust(int /*adjust*/)
{
}

void SDLRasterizer::setHorizontalScrollLow(byte /*scroll*/)
{
}

void SDLRasterizer::setBorderMask(bool /*masked*/)
{
}

void SDLRasterizer::setTransparency(bool enabled)
{
	spriteConverter.setTransparency(enabled);
	precalcColorIndex0(vdp.getDisplayMode(), enabled,
	                   vdp.isSuperimposing(), vdp.getBackgroundColor());
}

void SDLRasterizer::precalcPalette()
{
	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		const auto palette = vdp.getMSX1Palette();
		for (auto i : xrange(16)) {
			const auto rgb = palette[i];
			palFg[i] = palFg[i + 16] = palBg[i] =
				screen.mapRGB(
					renderSettings.transformRGB(
						vec3(rgb[0], rgb[1], rgb[2]) * (1.0f / 255.0f)));
		}
	} else {
		if (vdp.hasYJK()) {
			// Precalculate palette for V9958 colors.
			if (renderSettings.isColorMatrixIdentity()) {
				// Most users use the "normal" monitor type; making this a
				// special case speeds up palette precalculation a lot.
				std::array<int, 32> intensity;
				for (auto [i, r] : enumerate(intensity)) {
					r = narrow_cast<int>(255.0f * renderSettings.transformComponent(narrow<float>(i) * (1.0f / 31.0f)));
				}
				for (auto [rgb, col] : enumerate(V9958_COLORS)) {
					col = screen.mapRGB255(ivec3(
						intensity[(rgb >> 10) & 31],
						intensity[(rgb >>  5) & 31],
						intensity[(rgb >>  0) & 31]));
				}
			} else {
				for (auto r : xrange(32)) {
					for (auto g : xrange(32)) {
						for (auto b : xrange(32)) {
							vec3 rgb{narrow<float>(r),
							         narrow<float>(g),
							         narrow<float>(b)};
							V9958_COLORS[(r << 10) + (g << 5) + b] =
								screen.mapRGB(
									renderSettings.transformRGB(rgb * (1.0f / 31.0f)));
						}
					}
				}
			}
			// Precalculate palette for V9938 colors.
			// Based on comparing red and green gradients, using palette and
			// YJK, in SCREEN11 on a real turbo R.
			for (auto r3 : xrange(8)) {
				int r5 = (r3 << 2) | (r3 >> 1);
				for (auto g3 : xrange(8)) {
					int g5 = (g3 << 2) | (g3 >> 1);
					for (auto b3 : xrange(8)) {
						int b5 = (b3 << 2) | (b3 >> 1);
						V9938_COLORS[r3][g3][b3] =
							V9958_COLORS[(r5 << 10) + (g5 << 5) + b5];
					}
				}
			}
		} else {
			// Precalculate palette for V9938 colors.
			if (renderSettings.isColorMatrixIdentity()) {
				std::array<int, 8> intensity;
				for (auto [i, r] : enumerate(intensity)) {
					r = narrow_cast<int>(255.0f * renderSettings.transformComponent(narrow<float>(i) * (1.0f / 7.0f)));
				}
				for (auto r : xrange(8)) {
					for (auto g : xrange(8)) {
						for (auto b : xrange(8)) {
							V9938_COLORS[r][g][b] =
								screen.mapRGB255(ivec3(
									intensity[r],
									intensity[g],
									intensity[b]));
						}
					}
				}
			} else {
				for (auto r : xrange(8)) {
					for (auto g : xrange(8)) {
						for (auto b : xrange(8)) {
							vec3 rgb{narrow<float>(r),
							         narrow<float>(g),
							         narrow<float>(b)};
							V9938_COLORS[r][g][b] =
								screen.mapRGB(
									renderSettings.transformRGB(rgb * (1.0f / 7.0f)));
						}
					}
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (auto i : xrange(256)) {
			PALETTE256[i] = V9938_COLORS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[(i & 0x03) == 3 ? 7 : (i & 0x03) * 2];
		}
		// Precalculate Graphic 7 sprite palette.
		for (auto i : xrange(16)) {
			uint16_t grb = Renderer::GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLORS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

void SDLRasterizer::precalcColorIndex0(DisplayMode mode,
		bool transparency, const RawFrame* superimposing, byte bgColorIndex)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}

	int tpIndex = transparency ? bgColorIndex : 0;
	if (mode.getBase() != DisplayMode::GRAPHIC5) {
		Pixel c = (superimposing && (bgColorIndex == 0))
		        ? screen.getKeyColor()
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

std::pair<Pixel, Pixel> SDLRasterizer::getBorderColors()
{
	DisplayMode mode = vdp.getDisplayMode();
	int bgColor = vdp.getBackgroundColor();
	if (mode.getBase() == DisplayMode::GRAPHIC5) {
		// border in SCREEN6 has separate color for even and odd pixels.
		// TODO odd/even swapped?
		return {palBg[(bgColor & 0x0C) >> 2],
		        palBg[(bgColor & 0x03) >> 0]};
	}
	Pixel col = [&] { // other modes only have a single border color
		if (mode.getByte() == DisplayMode::GRAPHIC7) {
			return PALETTE256[bgColor];
		} else {
			if (!bgColor && vdp.isSuperimposing()) {
				return screen.getKeyColor();
			} else {
				return palBg[bgColor];
			}
		}
	}();
	return {col, col};
}

void SDLRasterizer::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	auto [border0, border1] = getBorderColors();

	int startY = std::max(fromY - lineRenderTop, 0);
	int endY = std::min(limitY - lineRenderTop, 240);
	if ((fromX == 0) && (limitX == VDP::TICKS_PER_LINE) &&
	    (border0 == border1)) {
		// complete lines, non striped
		for (auto y : xrange(startY, endY)) {
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
		for (auto y : xrange(startY, endY)) {
			MemoryOps::fill_2(workFrame->getLineDirect(y).subspan(x, num),
			                  border0, border1);
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

void SDLRasterizer::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	// Note: we don't call workFrame->setLineWidth() because that's done in
	// drawBorder() (for the right border). And the value we set there is
	// anyway the same as the one we would set here.
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
		//fromY = lineRenderTop;
		screenY = 0;
	}
	displayHeight = screenLimitY - screenY;
	if (displayHeight <= 0) return;

	int leftBackground =
		translateX(vdp.getLeftBackground(), lineWidth == 512);
	// TODO: Find out why this causes 1-pixel jitter:
	//dest.x = translateX(fromX);
	unsigned hScroll =
		  mode.isTextMode()
		? 0
		: 8 * (lineWidth / 256) * (vdp.getHorizontalScrollHigh() & 0x1F);

	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	int pageBorder = displayX + displayWidth;
	auto [scrollPage1, scrollPage2] = [&]() -> std::pair<int, int> {
		if (vdp.isMultiPageScrolling()) {
			int p1 = vdp.getHorizontalScrollHigh() >> 5;
			int p2 = p1 ^ 1;
			return {p1, p2};
		} else {
			return {0, 0};
		}
	}();
	// Because SDL blits do not wrap, unlike GL textures, the pageBorder is
	// also used if multi page is disabled.
	if (int pageSplit = narrow<int>(lineWidth - hScroll);
	    pageSplit < pageBorder) {
		pageBorder = pageSplit;
	}

	if (mode.isBitmapMode()) {
		for (auto y : xrange(screenY, screenLimitY)) {
			// Which bits in the name mask determine the page?
			// TODO optimize this?
			//   Calculating pageMaskOdd/Even is a non-trivial amount
			//   of work. We used to do this per frame (more or less)
			//   but now do it per line. Per-line is actually only
			//   needed when vdp.isFastBlinkEnabled() is true.
			//   Idea: can be cheaply calculated incrementally.
			unsigned pageMaskOdd = (mode.isPlanar() ? 0x000 : 0x200) |
				vdp.getEvenOddMask(y);
			unsigned pageMaskEven = vdp.isMultiPageScrolling()
				? (pageMaskOdd & ~0x100)
				: pageMaskOdd;
			const std::array<unsigned, 2> vramLine = {
				(vram.nameTable.getMask() >> 7) & (pageMaskEven | displayY),
				(vram.nameTable.getMask() >> 7) & (pageMaskOdd  | displayY)
			};

			std::array<Pixel, 512> buf;
			auto lineInBuf = unsigned(-1); // buffer data not valid
			auto dst = workFrame->getLineDirect(y).subspan(leftBackground + displayX);
			int firstPageWidth = pageBorder - displayX;
			if (firstPageWidth > 0) {
				if (((displayX + hScroll) == 0) &&
				    (firstPageWidth == narrow<int>(lineWidth))) {
					// fast-path, directly render to destination
					renderBitmapLine(dst, vramLine[scrollPage1]);
				} else {
					lineInBuf = vramLine[scrollPage1];
					renderBitmapLine(buf, vramLine[scrollPage1]);
					auto src = subspan(buf, displayX + hScroll, firstPageWidth);
					ranges::copy(src, dst);
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
				ranges::copy(subspan(buf, x, displayWidth - firstPageWidth),
				             subspan(dst, firstPageWidth));
			}

			displayY = (displayY + 1) & 255;
		}
	} else {
		// horizontal scroll (high) is implemented in CharacterConverter
		for (auto y : xrange(screenY, screenLimitY)) {
			assert(!vdp.isMSX1VDP() || displayY < 192);

			auto dst = workFrame->getLineDirect(y).subspan(leftBackground + displayX);
			if ((displayX == 0) && (displayWidth == narrow<int>(lineWidth))){
				characterConverter.convertLine(dst, displayY);
			} else {
				std::array<Pixel, 512> buf;
				characterConverter.convertLine(buf, displayY);
				auto src = subspan(buf, displayX, displayWidth);
				ranges::copy(src, dst);
			}

			displayY = (displayY + 1) & 255;
		}
	}
}

void SDLRasterizer::drawSprites(
	int /*fromX*/, int fromY,
	int displayX, int /*displayY*/,
	int displayWidth, int displayHeight)
{
	// Clip to screen area.
	// TODO: Code duplicated from drawDisplay.
	int screenLimitY = std::min(
		fromY + displayHeight - lineRenderTop,
		240);
	int screenY = fromY - lineRenderTop;
	if (screenY < 0) {
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
			auto dst = workFrame->getLineDirect(screenY).subspan(screenX);
			spriteConverter.drawMode1(y, displayX, displayLimitX, dst);
		}
	} else {
		byte mode = vdp.getDisplayMode().getByte();
		if (mode == DisplayMode::GRAPHIC5) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				auto dst = workFrame->getLineDirect(screenY).subspan(screenX);
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC5>(
					y, displayX, displayLimitX, dst);
			}
		} else if (mode == DisplayMode::GRAPHIC6) {
			for (int y = fromY; y < limitY; y++, screenY++) {
				auto dst = workFrame->getLineDirect(screenY).subspan(screenX);
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC6>(
					y, displayX, displayLimitX, dst);
			}
		} else {
			for (int y = fromY; y < limitY; y++, screenY++) {
				auto dst = workFrame->getLineDirect(screenY).subspan(screenX);
				spriteConverter.template drawMode2<DisplayMode::GRAPHIC4>(
					y, displayX, displayLimitX, dst);
			}
		}
	}
}

bool SDLRasterizer::isRecording() const
{
	return postProcessor->isRecording();
}

void SDLRasterizer::update(const Setting& setting) noexcept
{
	if (&setting == one_of(&renderSettings.getGammaSetting(),
	                       &renderSettings.getBrightnessSetting(),
	                       &renderSettings.getContrastSetting(),
	                       &renderSettings.getColorMatrixSetting())) {
		precalcPalette();
		resetPalette();
	}
}

} // namespace openmsx
