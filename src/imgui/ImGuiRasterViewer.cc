#include "ImGuiRasterViewer.hh"

#include "MSXMotherBoard.hh"
#include "RawFrame.hh"
#include "VDP.hh"

#include "ImGuiCpp.hh"

#include "MemBuffer.hh"
#include "gl_vec.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"

#include <imgui.h>

#include <algorithm>
#include <cassert>

namespace openmsx {

using namespace std::literals;

using Pixel = uint32_t;
static constexpr auto PIXELS_PER_LINE = VDP::TICKS_PER_LINE / 2;
static constexpr auto VISIBLE_PIXELS_PER_LINE = 28 + 512 + 30;
static_assert(VISIBLE_PIXELS_PER_LINE % 2 == 0, "must be even");
static constexpr auto FIRST_VISIBLE_LINE = 3 + 13;
static constexpr auto FIRST_VISIBLE_X = (100 + 102) / 2;

ImGuiRasterViewer::ImGuiRasterViewer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
}

void ImGuiRasterViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiRasterViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiRasterViewer::initPixelBuffer(size_t lines)
{
	size_t size = lines * VISIBLE_PIXELS_PER_LINE;
	if (pixelBuffer.size() == size) return;
	pixelBuffer.resize(size);
}

[[nodiscard]] static std::span<Pixel, VISIBLE_PIXELS_PER_LINE> getVisibleLinePart(
	std::span<Pixel> pixels, size_t line)
{
	assert(line >= FIRST_VISIBLE_LINE);
	line -= FIRST_VISIBLE_LINE;
	return subspan<VISIBLE_PIXELS_PER_LINE>(pixels, line * VISIBLE_PIXELS_PER_LINE);
}

static unsigned copyLine(const RawFrame& src, int openMsxLine, std::span<Pixel, VISIBLE_PIXELS_PER_LINE> dst)
{
	// About the source line:
	// - OpenMSX renders lines of width 320 (or 640) pixels, but the real VDP
	//   renders 14/15 (or 28/30) border pixels left/right plus 256 (or 512) display
	//   pixels.
	// - This means the openMSX lines contains 18 (or 36) pixels too many on both sides.
	// - With horizontal adjust there can be a shift between left and right border.
	// About the destination line:
	// - We want to display all 1368 cycles (corresponding to 684 pixels).
	// - A line is subdivided in these parts:
	//   *  100 cycles ( 25    or  50   pixels): sync
	//   *  102 cycles ( 25.5  or  51   pixels): left erase
	//   *   56 cycles ( 14    or  28   pixels): left border
	//   * 1024 cycles (256    or 512   pixels): display area
	//   *   59 cycles ( 14.75 or  29.5 pixels): right border
	//   *   27 cycles (  6.75 or  13.5 pixels): right erase
	// - We render 684 pixels, so skip 101 pixels, 28 border pixels, 512
	//   display pixels, 30 border pixels, and 13 pixels remaining.
	int line = std::clamp(openMsxLine, 0, 239);
	auto srcLine = src.getLineDirect(line);
	auto width = src.getLineWidthDirect(line);
	switch (width) {
	case 1:
		std::ranges::fill(dst, srcLine[0]);
		break;
	case 320:
		for (size_t i = 0; i < (VISIBLE_PIXELS_PER_LINE / 2); ++i) {
			dst[2 * i + 0] = srcLine[i + 18];
			dst[2 * i + 1] = srcLine[i + 18];
		}
		break;
	case 640:
		copy_to_range(subspan<VISIBLE_PIXELS_PER_LINE>(srcLine, 36), dst);
		break;
	default:
		UNREACHABLE;
	}
	return width;
}

void ImGuiRasterViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	ImGui::SetNextWindowSize({700, 722}, ImGuiCond_FirstUseEver);
	im::Window("Raster beam viewer", &show, [&]{
		auto* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp) return;

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			paintSettings();
		});
		ImGui::Separator();
		paintDisplay(vdp);
	});
}

static const int MAX_ZOOM = 8;
void ImGuiRasterViewer::paintSettings()
{
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
	ImGui::Combo("Zoom", &zoomSelect, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
	assert(MAX_ZOOM == 8);

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	ImGui::Checkbox("Grid", &showGrid);
	ImGui::SameLine();
	im::Disabled(!showGrid, [&]{
		ImGui::ColorEdit4("Grid color", gridColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("At zoom 1x the grid isn't shown for hi-res screen modes (e.g. screen 7)");

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	ImGui::Checkbox("Fade out", &showFadeOut);
	ImGui::SameLine();
	im::Disabled(!showFadeOut, [&]{
		ImGui::ColorEdit4("FadeOut color", fadeOutColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("Similar to the 'beam position': shows the current position "
	           "of the CRT raster beam, but now by fading out 'older' colors");

	ImGui::Checkbox("Beam position", &showBeam);
	ImGui::SameLine();
	im::Disabled(!showBeam, [&]{
		ImGui::ColorEdit4("Beam color", beamColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("Shows the current position of the CRT raster beam. "
	           "In general this is only useful when emulation is paused, "
	           "e.g. when stepping through the code with the debugger.");

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	ImGui::Checkbox("VBlank IRQ", &showVblankIrq);
	ImGui::SameLine();
	im::Disabled(!showVblankIrq, [&]{
		ImGui::ColorEdit4("VBlank color", vblankIrqColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("Shows the position where the vblank-IRQ will occur (when enabled)");

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	ImGui::Checkbox("Line IRQ", &showLineIrq);
	ImGui::SameLine();
	im::Disabled(!showLineIrq, [&]{
		ImGui::ColorEdit4("Line color", lineIrqColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("Shows the position where the line-IRQ will occur (when enabled)");

}

void ImGuiRasterViewer::paintDisplay(VDP* vdp)
{
	EmuTime time = vdp->getCurrentTime();
	const RawFrame* work = vdp->getWorkingFrame(time);
	const RawFrame* last = vdp->getLastFrame();
	if (!work || !last) return;
	assert(work->getHeight() == 240);
	assert(last->getHeight() == 240);

	auto timeToXY = [&](EmuTime t) -> std::pair<int, int> {
		int ticks = vdp->getTicksThisFrame(t);
		return {ticks % VDP::TICKS_PER_LINE,
		        ticks / VDP::TICKS_PER_LINE};
	};
	auto [x, y] = timeToXY(time);

	// Openmsx renders into a 240 line RawFrame (212 or 192 display + borders).
	// That means:
	// - For neutral vertical adjust (0), there are 14 border lines at the top,
	//   212 display lines, and 14 border lines at the bottom.
	// - With vertical adjust in the top most position, the display
	//   area shifts 7 pixels to the top, so 7 + 212 + 21.
	// - With vertical adjust in the bottom most position, the display
	//   area shifts 8 pixels to the bottom, so 22 + 212 + 6.
	// - In case of only 192 display lines, there are 20 more border lines,
	//   10 above and 10 below.
	bool pal = vdp->isPalTiming();
	int numLines = pal ? VDP::PAL_LINES : VDP::NTSC_LINES;

	int zoom = zoomSelect + 1;
	gl::vec2 fullSize{float(PIXELS_PER_LINE), float(2 * numLines)};
	gl::vec2 zoomedFullSize = fullSize * float(zoom);

	auto availSize = gl::vec2(ImGui::GetContentRegionAvail()) - gl::vec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
	auto reqSize = zoomedFullSize + gl::vec2(ImGui::GetStyle().ScrollbarSize);
	gl::vec2 scrnPos;

	std::array<unsigned, VDP::NUM_LINES_MAX> allLineWidths;
	std::ranges::fill(allLineWidths, 1); // default to border width

	im::Child("##display", min(availSize, reqSize), {}, ImGuiWindowFlags_HorizontalScrollbar, [&]{
		scrnPos = ImGui::GetCursorScreenPos();

		auto makeAndBindTex = [&](gl::Texture& tex) {
			if (!tex.get()) {
				tex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			tex.bind();
		};

		gl::vec4 checkerBoardColor1{0.2f, 0.2f, 0.2f, 0.8f}; // RGBA
		gl::vec4 checkerBoardColor2{0.4f, 0.4f, 0.4f, 0.8f}; // RGBA
		auto col1 = ImGui::ColorConvertFloat4ToU32(checkerBoardColor1);
		auto col2 = ImGui::ColorConvertFloat4ToU32(checkerBoardColor2);
		std::array<Pixel, 4> pixels = {col1, col2, col2, col1};
		makeAndBindTex(checkerTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// align checkerboard pattern with MSX pixel (0,0) (when set-adjust is 0)
		static constexpr float checkerBoardSize = 8.0f;
		static constexpr gl::vec2 texOffset = gl::vec2{-1.0f, 10.0f} / (4.0f * checkerBoardSize);
		ImGui::Image(checkerTex.getImGui(), zoomedFullSize,
		             texOffset, texOffset + fullSize / (4.0f * checkerBoardSize));

		static constexpr int visibleLinesPal = 43 + 212 + 39;
		static constexpr int visibleLinesNtsc = 16 + 212 + 15;
		int numVisibleLines = pal ? visibleLinesPal : visibleLinesNtsc;
		initPixelBuffer(numVisibleLines);
		auto lineWidths = std::span(allLineWidths).subspan(FIRST_VISIBLE_LINE, numVisibleLines);
		bool any320 = false;
		bool any640 = false;
		auto setWidth = [&](int i, unsigned width) {
			lineWidths[i] = width;
			if (width == 320) any320 = true;
			if (width == 640) any640 = true;
		};

		int firstOpenMsxLine = FIRST_VISIBLE_LINE + ((pal ? 43 : 16) - 14);
		for (auto i : xrange(numVisibleLines)) {
			auto absLine = i + FIRST_VISIBLE_LINE;
			auto dst = getVisibleLinePart(pixelBuffer, absLine);
			int openMsxLine = absLine - firstOpenMsxLine;
			if (absLine < y) {
				auto w = copyLine(*work, openMsxLine, dst);
				setWidth(i, w);
			} else if (absLine > y) {
				auto w = copyLine(*last, openMsxLine, dst);
				setWidth(i, w);
			} else {
				std::array<Pixel, VISIBLE_PIXELS_PER_LINE> left;
				std::array<Pixel, VISIBLE_PIXELS_PER_LINE> right;
				auto wl = copyLine(*work, openMsxLine, left);
				auto wr = copyLine(*last, openMsxLine, right);
				setWidth(i, std::max(wl, wr));

				int xx = (x - (100 + 102)) / 2;
				if (wl <= 320) xx &= ~1; // round down to even
				int n = std::clamp(xx, 0, VISIBLE_PIXELS_PER_LINE);

				copy_to_range(subspan(left, 0, n), dst.subspan(0, n));
				copy_to_range(subspan(right, n),   dst.subspan(n));
			}
		}
		// Set border lines (width=1):
		//  to 640 if there are lines with width=640 and there are none with width=320,
		//  else to 320
		int borderWidth = any640 && !any320 ? 640 : 320;
		if (borderWidth == 320) any320 = true;
		std::ranges::replace(allLineWidths, 1, borderWidth);

		makeAndBindTex(viewTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VISIBLE_PIXELS_PER_LINE, numVisibleLines, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer.data());

		auto* drawList = ImGui::GetWindowDrawList();
		auto colorForLine = [&](int line) {
			auto color = [&] {
				static constexpr gl::vec4 opaque(1.0f);
				if (!showFadeOut) return opaque;
				float t = float(line - y) / float(numLines);
				return ImLerp(fadeOutColor, opaque, t - std::floor(t));
			}();
			return ImGui::ColorConvertFloat4ToU32(color);
		};
		auto topColor = colorForLine(FIRST_VISIBLE_LINE);
		auto bottomLine = FIRST_VISIBLE_LINE + numVisibleLines;
		auto bottomColor = colorForLine(bottomLine);
		auto topLeft = scrnPos + float(zoom) * gl::vec2{float(FIRST_VISIBLE_X), float(FIRST_VISIBLE_LINE * 2)};
		auto bottomRight = topLeft + float(zoom) * gl::vec2{float(VISIBLE_PIXELS_PER_LINE), float(2 * numVisibleLines)};
		if (y < FIRST_VISIBLE_LINE || y >= bottomLine) {
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				topLeft, bottomRight,
				gl::vec2{0.0f, 0.0f}, gl::vec2{1.0f, 1.0f},
				topColor, topColor, bottomColor, bottomColor);
		} else {
			auto middleVertexY = scrnPos.y + float(2 * zoom * y);
			auto middleTexY = float(y - FIRST_VISIBLE_LINE) / float(numVisibleLines);
			Pixel opaque = 0xffffffff;

			// top gradient
			if (y != FIRST_VISIBLE_LINE) {
				ImGui::AddImageRectMultiColor(
					drawList, viewTex.getImGui(),
					topLeft, gl::vec2{bottomRight.x, middleVertexY},
					gl::vec2{0.0f, 0.0f}, gl::vec2{1.0f, middleTexY},
					topColor, topColor, opaque, opaque);
			}
			// middle line
			int x1 = (x / 2) - FIRST_VISIBLE_X;
			if (allLineWidths[y] == 320) x1 &= ~1; // round down to even
			auto x2 = std::clamp(topLeft.x + float(zoom * x1), topLeft.x, bottomRight.x);
			auto t2 = std::clamp(float(x1) / float(VISIBLE_PIXELS_PER_LINE), 0.0f, 1.0f);
			auto middleVertexY1 = middleVertexY + float(2 * zoom);
			auto middleTexY1 = float((y + 1) - FIRST_VISIBLE_LINE) / float(numVisibleLines);
			drawList->AddImage(
				viewTex.getImGui(),
				gl::vec2{topLeft.x, middleVertexY}, gl::vec2{x2, middleVertexY1},
				gl::vec2{0.0f, middleTexY}, gl::vec2{t2, middleTexY1},
				opaque);
			Pixel transparent = colorForLine(y);
			drawList->AddImage(
				viewTex.getImGui(),
				gl::vec2{x2, middleVertexY}, gl::vec2{bottomRight.x, middleVertexY1},
				gl::vec2{t2, middleTexY}, gl::vec2{1.0f, middleTexY1},
				transparent);
			// bottom gradient
			if (y != (bottomLine - 1)) {
				ImGui::AddImageRectMultiColor(
					drawList, viewTex.getImGui(),
					gl::vec2{topLeft.x, middleVertexY1}, bottomRight,
					gl::vec2{0.0f, middleTexY1}, gl::vec2{1.0f, 1.0f},
					transparent, transparent, bottomColor, bottomColor);
			}
		}

		if (showGrid) {
			std::array<Pixel, (2 * MAX_ZOOM) * (2 * MAX_ZOOM)> pixels_;
			auto color = ImGui::ColorConvertFloat4ToU32(gridColor);
			auto makeGridUnit = [&](int nx, int ny) {
				auto pixels2 = std::span(pixels_).subspan(0, nx * ny);
				for (auto yy : xrange(ny)) {
					auto line = pixels2.subspan(yy * nx, nx);
					for (auto xx : xrange(nx)) {
						line[xx] = (xx == 0 || yy == 0) ? color : 0;
					}
				}
			};
			if (any320) {
				int nx = 2 * zoom;
				int ny = 2 * zoom;
				makeGridUnit(nx, ny);
				makeAndBindTex(gridTex320);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
			}
			if (any640 && (zoom > 1)) {
				int nx = zoom;
				int ny = 2 * zoom;
				makeGridUnit(nx, ny);
				makeAndBindTex(gridTex640);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
			}
			foreach_run(lineWidths, [&](size_t i, size_t n, unsigned width) {
				assert(width == one_of(320u, 640u));
				if (width == 640 && (zoom <= 1)) return;
				auto numPixels = VISIBLE_PIXELS_PER_LINE / ((width == 320) ? 2 : 1);
				drawList->AddImage(
					((width == 320) ? gridTex320 : gridTex640).getImGui(),
					gl::vec2{topLeft.x, topLeft.y + float(2 * zoom * i)},
					gl::vec2{bottomRight.x, topLeft.y + float(2 * zoom * (i + n))},
					gl::vec2{}, gl::vec2{float(numPixels), float(n)});
			});
		}

		if (showBeam) {
			drawCrosshair(x, y, beamColor, scrnPos, zoom, allLineWidths);
		}
		if (showVblankIrq) {
			if (auto vTime = vdp->getVScanTime()) {
				auto [vx, vy] = timeToXY(*vTime);
				vy %= numLines;
				drawCrosshair(vx, vy, vblankIrqColor, scrnPos, zoom, allLineWidths);
			}
		}
		if (showLineIrq) {
			if (auto hTime = vdp->getHScanTime()) {
				auto [hx, hy] = timeToXY(*hTime);
				hy %= numLines;
				drawCrosshair(hx, hy, lineIrqColor, scrnPos, zoom, allLineWidths);
			}
		}
	});
	if (ImGui::IsItemHovered()) {
		auto [tx, vy] = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / (gl::vec2(0.5f, 2.0f) * float(zoom)));
		if (0 <= tx && tx < VDP::TICKS_PER_LINE &&
		    0 <= vy && vy < numLines) {
			int vx = tx / 2;
			int mx = (tx - vdp->getLeftSprites()) / 2;
			int my = vy - vdp->getLineZero();
			if (allLineWidths[vy] == 320) {
				vx >>= 1;
				mx >>= 1; // note: NOT the same as 'mx / 2' for negative values
			}

			auto dec3 = [&](int d) {
				im::ScopedFont sf(manager.fontMono);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::Text("%3d", d);
			};

			ImGui::TextUnformatted("MSX coordinates: x="sv); dec3(mx);
			ImGui::SameLine();
			ImGui::TextUnformatted("y="sv); dec3(my);

			ImGui::SameLine(0.0f, 20.0f);
			ImGui::TextUnformatted("Absolute VDP coordinates: x="sv); dec3(vx);
			ImGui::SameLine();
			ImGui::TextUnformatted("line="sv); dec3(vy);
		}
	}
}

void ImGuiRasterViewer::drawCrosshair(
	int x, int y, gl::vec4 color_, gl::vec2 scrnPos, int zoom,
	std::span<const unsigned, VDP::NUM_LINES_MAX> lineWidths)
{
	auto xx = x / 2;
	auto zx = 0.5f;
	auto cx = 0.5f;
	if (lineWidths[y] == 320) {
		xx = (xx + 1) & ~1; // round up to even
		zx = 1.0f;
		cx = 0.25f;
	}
	gl::vec2 rasterBeamPos(float(xx), float(y) * 2.0f);
	auto thickness = float(zoom) * 0.5f;
	auto center = scrnPos + (gl::vec2(rasterBeamPos) + gl::vec2{cx, 1.0f}) * float(zoom);
	gl::vec2 zm = float(zoom) * gl::vec2(zx, 1.0f);
	auto zm1 = 1.5f * zm;
	auto zm3 = 3.5f * zm;
	auto* drawList = ImGui::GetWindowDrawList();
	auto color = ImGui::ColorConvertFloat4ToU32(color_);
	drawList->AddRect(center - zm, center + zm, color, 0.0f, 0, thickness);
	drawList->AddLine(center - gl::vec2{zm1.x, 0.0f}, center - gl::vec2{zm3.x, 0.0f}, color, thickness);
	drawList->AddLine(center + gl::vec2{zm1.x, 0.0f}, center + gl::vec2{zm3.x, 0.0f}, color, thickness);
	drawList->AddLine(center - gl::vec2{0.0f, zm1.y}, center - gl::vec2{0.0f, zm3.y}, color, thickness);
	drawList->AddLine(center + gl::vec2{0.0f, zm1.y}, center + gl::vec2{0.0f, zm3.y}, color, thickness);
}

} // namespace openmsx
