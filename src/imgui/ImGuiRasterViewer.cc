#include "ImGuiRasterViewer.hh"

#include "MSXMotherBoard.hh"
#include "RawFrame.hh"
#include "VDP.hh"

#include "ImGuiCpp.hh"

#include "MemBuffer.hh"
#include "gl_vec.hh"
#include "ranges.hh"
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

[[nodiscard]] static std::span<uint32_t, VISIBLE_PIXELS_PER_LINE> getVisibleLinePart(
	std::span<uint32_t> pixels, size_t line)
{
	assert(line >= FIRST_VISIBLE_LINE);
	line -= FIRST_VISIBLE_LINE;
	return subspan<VISIBLE_PIXELS_PER_LINE>(pixels, line * VISIBLE_PIXELS_PER_LINE);
}

static bool copyLine(const RawFrame& src, int openMsxLine, std::span<uint32_t, VISIBLE_PIXELS_PER_LINE> dst)
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
		return false;
	case 320:
		for (size_t i = 0; i < (14 + 256 + 14); ++i) {
			dst[2 * i + 0] = srcLine[i + 18];
			dst[2 * i + 1] = srcLine[i + 18];
		}
		return true;
	case 640:
		copy_to_range(subspan<28 + 512 + 28>(srcLine, 36), dst);
		return false;
	default:
		UNREACHABLE;
	}
}

void ImGuiRasterViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	//ImGui::SetNextWindowSize({528, 620}, ImGuiCond_FirstUseEver);
	im::Window("Raster Viewer (prototype)", &show, [&]{
		auto* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp) return;
		EmuTime time = motherBoard->getCurrentTime();
		const RawFrame* work = vdp->getWorkingFrame(time);
		const RawFrame* last = vdp->getLastFrame();
		if (!work || !last) return;
		assert(work->getHeight() == 240);
		assert(last->getHeight() == 240);

		int ticks = vdp->getTicksThisFrame(time);
		int x = ticks % VDP::TICKS_PER_LINE;
		int y = ticks / VDP::TICKS_PER_LINE;
		ImGui::Text("x=%d y=%d ticks=%d", x, y, ticks);

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

		auto scrnPos = ImGui::GetCursorScreenPos();
		if (true /*fullFrame*/) {
			std::array<uint32_t, 4> pixels;
			gl::vec4 checkerBoardColor1{0.2f, 0.2f, 0.2f, 0.8f}; // RGBA
			gl::vec4 checkerBoardColor2{0.4f, 0.4f, 0.4f, 0.8f}; // RGBA
			pixels[0] = pixels[3] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor1);
			pixels[1] = pixels[2] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor2);
			if (!checkerTex.get()) {
				checkerTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			checkerTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

			gl::vec2 fullSize{float(PIXELS_PER_LINE), float(2 * numLines)};
			int checkerBoardSize = 8;
			ImGui::Image(checkerTex.getImGui(), fullSize,
			             {}, fullSize / (4.0f * float(checkerBoardSize)));
		}
		int numVisibleLines = pal ? (43 + 212 + 39) : (16 + 212 + 15);
		initPixelBuffer(numVisibleLines);

		int firstOpenMsxLine = FIRST_VISIBLE_LINE + ((pal ? 43 : 16) - 14);
		for (auto i : xrange(FIRST_VISIBLE_LINE, FIRST_VISIBLE_LINE + numVisibleLines)) {
			auto dst = getVisibleLinePart(pixelBuffer, i);
			int openMsxLine = i - firstOpenMsxLine;
			if (i < y) {
				copyLine(*work, openMsxLine, dst);
			} else if (i > y) {
				copyLine(*last, openMsxLine, dst);
			} else {
				std::array<uint32_t, VISIBLE_PIXELS_PER_LINE> left;
				std::array<uint32_t, VISIBLE_PIXELS_PER_LINE> right;
				bool w256 = copyLine(*work, openMsxLine, left);
				copyLine(*last, openMsxLine, right);

				int xx = (x - (100 + 102)) / 2;
				if (w256) xx &= ~1; // round down to even
				int n = std::clamp(xx, 0, 28 + 512 + 28);

				copy_to_range(subspan(left, 0, n), dst.subspan(0, n));
				copy_to_range(subspan(right, n),   dst.subspan(n));
			}
		}

		if (!viewTex.get()) {
			viewTex = gl::Texture(false, false); // no interpolation, no wrapping
		}
		viewTex.bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VISIBLE_PIXELS_PER_LINE, numVisibleLines, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer.data());

		auto* drawList = ImGui::GetWindowDrawList();
		auto colorForLine = [&](int line) {
			float fadeOut = 0.75f;
			float t = float(((numLines - y) + line) % numLines) / float(numLines);
			float t2 = std::lerp(fadeOut, 1.0f, t);
			return uint32_t(ImColor(1.0f, 1.0f, 1.0f, t2));
		};
		auto topColor = colorForLine(FIRST_VISIBLE_LINE);
		auto bottomLine = FIRST_VISIBLE_LINE + numVisibleLines;
		auto bottomColor = colorForLine(bottomLine);
		auto topLeft = scrnPos + gl::vec2{float(FIRST_VISIBLE_X), float(FIRST_VISIBLE_LINE * 2)};
		auto bottomRight = topLeft + gl::vec2{float(VISIBLE_PIXELS_PER_LINE), float(2 * numVisibleLines)};
		if (y < FIRST_VISIBLE_LINE || y >= bottomLine) {
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				topLeft, bottomRight,
				gl::vec2{0.0f, 0.0f}, gl::vec2{1.0f, 1.0f},
				topColor, topColor, bottomColor, bottomColor);
		} else {
			auto middleVertexY = scrnPos.y + float(2 * y);
			auto middleTexY = float(y - FIRST_VISIBLE_LINE) / float(numVisibleLines);
			uint32_t opaque = 0xffffffff;
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				topLeft, gl::vec2{bottomRight.x, middleVertexY},
				gl::vec2{0.0f, 0.0f}, gl::vec2{1.0f, middleTexY},
				topColor, topColor, opaque, opaque);
			uint32_t transparent = colorForLine(y);
			ImGui::AddImageRectMultiColor(
				drawList, viewTex.getImGui(),
				gl::vec2{topLeft.x, middleVertexY}, bottomRight,
				gl::vec2{0.0f, middleTexY}, gl::vec2{1.0f, 1.0f},
				transparent, transparent, bottomColor, bottomColor);
		}

		if (true /*rasterBeam*/) {
			gl::vec2 rasterBeamPos(float(x / 2), float(y * 2));
			gl::vec2 zm(1.0f); // ?
			auto center = scrnPos + (gl::vec2(rasterBeamPos) + gl::vec2{0.5f}) * zm;
			gl::vec4 rasterBeamColor{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
			auto color = ImGui::ColorConvertFloat4ToU32(rasterBeamColor);
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
}

} // namespace openmsx
