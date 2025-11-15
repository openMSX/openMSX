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

static constexpr auto PIXELS_PER_LINE = VDP::TICKS_PER_LINE / 2;
static constexpr auto VISIBLE_PIXELS_PER_LINE = 28 + 512 + 28;
static constexpr auto FIRST_VISIBLE_LINE = 3 + 13;
static constexpr auto FIRST_VISIBLE_X = 102; // in pixels
static constexpr auto PAL_LINES = 313; // TODO move to VDP
static constexpr auto NTSC_LINES = 262;

using Pixel = uint32_t;

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
	//   renders 14 (or 28) border pixels left and right plus 256 (or 512) display
	//   pixels.
	// - This means the openMSX lines contains 18 (or 36) pixels too many on both sides.
	// - With horizontal adjust there can be a shift between left and right border.
	// About the destination line:
	// - We want to display all 1368 cycles (corresponding to 684 pixels).
	// - A line is subdivided in these parts:
	//   *  100 cycles ( 25 or  51 pixels): sync
	//   *  102 cycles ( 25 or  51 pixels): left erase
	//   *   56 cycles ( 14 or  28 pixels): left border
	//   * 1024 cycles (256 or 512 pixels): display area
	//   *   59 cycles ( 14 or  28 pixels): right border
	//   *   27 cycles (  8 or  14 pixels): right erase
	// - We render 684 pixels, so skip 102 pixels, 28 border pixels, 512
	//   display pixels, 28 border pixels, and 14 pixels remaining.
	int line = std::clamp(openMsxLine, 0, 239);
	auto srcLine = src.getLineDirect(line);
	auto width = src.getLineWidthDirect(line);
	switch (width) {
	case 1:
		std::ranges::fill(dst, srcLine[0]);
		break;
	case 320:
		for (size_t i = 0; i < (14 + 256 + 14); ++i) {
			dst[2 * i + 0] = srcLine[i + 18];
			dst[2 * i + 1] = srcLine[i + 18];
		}
		break;
	case 640:
		copy_to_range(subspan<28 + 512 + 28>(srcLine, 36), dst);
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

	ImGui::Checkbox("Fade out", &showFadeOut);
	ImGui::SameLine();
	im::Disabled(!showFadeOut, [&]{
		ImGui::ColorEdit4("FadeOut color", fadeOutColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
	HelpMarker("Similar to the 'beam position': shows the current position "
	           "of the CRT raster beam, but now by fading out 'older' colors");
}

void ImGuiRasterViewer::paintDisplay(VDP* vdp)
{
	EmuTime time = vdp->getCurrentTime();
	const RawFrame* work = vdp->getWorkingFrame(time);
	const RawFrame* last = vdp->getLastFrame();
	if (!work || !last) return;
	assert(work->getHeight() == 240);
	assert(last->getHeight() == 240);

	int ticks = vdp->getTicksThisFrame(time);
	int x = ticks % VDP::TICKS_PER_LINE;
	int y = ticks / VDP::TICKS_PER_LINE;

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
	int numLines = pal ? PAL_LINES : NTSC_LINES;

	int zoom = zoomSelect + 1;
	gl::vec2 fullSize{float(PIXELS_PER_LINE), float(2 * numLines)};
	gl::vec2 zoomedFullSize = fullSize * float(zoom);

	auto availSize = gl::vec2(ImGui::GetContentRegionAvail()) - gl::vec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
	auto reqSize = zoomedFullSize + gl::vec2(ImGui::GetStyle().ScrollbarSize);
	im::Child("##display", min(availSize, reqSize), {}, ImGuiWindowFlags_HorizontalScrollbar, [&]{
		auto scrnPos = ImGui::GetCursorScreenPos();

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

		int checkerBoardSize = 8;
		ImGui::Image(checkerTex.getImGui(), zoomedFullSize,
				{}, fullSize / (4.0f * float(checkerBoardSize)));

		static constexpr int visibleLinesPal = 43 + 212 + 39;
		static constexpr int visibleLinesNtsc = 43 + 212 + 39;
		static constexpr int visibleLinesMax = std::max(visibleLinesPal, visibleLinesNtsc);
		int numVisibleLines = pal ? visibleLinesPal : visibleLinesNtsc;
		initPixelBuffer(numVisibleLines);
		std::array<unsigned, visibleLinesMax> lineWidths_;
		auto lineWidths = std::span(lineWidths_).subspan(0, numVisibleLines);
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
				int n = std::clamp(xx, 0, 28 + 512 + 28);

				copy_to_range(subspan(left, 0, n), dst.subspan(0, n));
				copy_to_range(subspan(right, n),   dst.subspan(n));
			}
		}
		// Set border lines (width=1):
		//  to 640 if there are lines with width=640 and there are none with width=320,
		//  else to 320
		int borderWidth = any640 && !any320 ? 640 : 320;
		if (borderWidth == 320) any320 = true;
		std::ranges::replace(lineWidths, 1, borderWidth);

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
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				topLeft, gl::vec2{bottomRight.x, middleVertexY},
				gl::vec2{0.0f, 0.0f}, gl::vec2{1.0f, middleTexY},
				topColor, topColor, opaque, opaque);
			// TODO split middle line
			Pixel transparent = colorForLine(y);
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				gl::vec2{topLeft.x, middleVertexY}, bottomRight,
				gl::vec2{0.0f, middleTexY}, gl::vec2{1.0f, 1.0f},
				transparent, transparent, bottomColor, bottomColor);
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
			gl::vec2 rasterBeamPos(float(x) * 0.5f, float(y) * 2.0f);
			gl::vec2 zm = float(zoom) * gl::vec2(1.0f); // ?
			auto center = scrnPos + (gl::vec2(rasterBeamPos) + gl::vec2{0.5f}) * zm;
			auto color = ImGui::ColorConvertFloat4ToU32(beamColor);
			auto thickness = zm.y * 0.5f;
			auto zm1 = 1.5f * zm;
			auto zm3 = 3.5f * zm;
			drawList->AddRect(center - zm, center + zm, color, 0.0f, 0, thickness);
			drawList->AddLine(center - gl::vec2{zm1.x, 0.0f}, center - gl::vec2{zm3.x, 0.0f}, color, thickness);
			drawList->AddLine(center + gl::vec2{zm1.x, 0.0f}, center + gl::vec2{zm3.x, 0.0f}, color, thickness);
			drawList->AddLine(center - gl::vec2{0.0f, zm1.y}, center - gl::vec2{0.0f, zm3.y}, color, thickness);
			drawList->AddLine(center + gl::vec2{0.0f, zm1.y}, center + gl::vec2{0.0f, zm3.y}, color, thickness);
		}
	});
	if (ImGui::IsItemHovered()) {
		ImGui::TextUnformatted("TODO: show coordinates on hover");
	}
}

} // namespace openmsx
