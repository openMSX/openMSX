#include "ImGuiBitmapViewer.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiUtils.hh"

#include "DisplayMode.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "MemBuffer.hh"
#include "ranges.hh"

#include <imgui.h>

namespace openmsx {

using namespace std::literals;

// Take VDPCmdEngine info like {sx,sy} (or {dx,dy}), {nx,ny}, ...
// And turn that into a rectangle that's properly clipped horizontally for the
// given screen mode. Taking into account pixel vs byte mode.

// Usually this returns just 1 retangle, but in case of vertical wrapping this
// can be split over two (disjunct) rectangles. In that case the order in which
// the VDP handles these rectangles (via 'diy') is preserved.
//
// Also within a single rectangle the VDP order (via 'dix' and 'diy') is
// preserved. So start at corner 'p1', end at corner 'p2'.
//
// Both start and end points of the rectangle(s) are inclusive.
static_vector<Rect, 2> rectFromVdpCmd(
	int x, int y, // either sx,sy or dx,dy
	int nx, int ny,
	bool dix, bool diy,
	ImGuiBitmapViewer::ScrnMode screenMode,
	bool byteMode) // Lxxx or Hxxx command
{
	const auto [width, height, pixelsPerByte] = [&] {
		switch (screenMode) {
		using enum ImGuiBitmapViewer::ScrnMode;
		case SCR5: return std::tuple{256, 1024, 2};
		case SCR6: return std::tuple{512, 1024, 4};
		case SCR7: return std::tuple{512,  512, 2};
		default:   return std::tuple{256,  512, 1}; // screen 8, 11, 12  (and fallback for non-bitmap)
		}
	}();

	// clamp/wrap start-point
	x = std::clamp(x, 0, width - 1); // Clamp to border. This is different from the real VDP behavior.
	                            // But for debugging it's visually less confusing??
	y &= (height - 1); // wrap around

	if (nx <= 0) nx = width;
	if (ny <= 0) ny = height;
	if (byteMode) { // round to byte positions
		auto mask = ~(pixelsPerByte - 1);
		x &= mask;
		nx &= mask;
	}
	nx -= 1; // because coordinates are inclusive
	ny -= 1;

	// horizontally clamp to left/right border
	auto endX = std::clamp(x + (dix ? -nx : nx), 0, width - 1);

	// but vertically wrap around, possibly that splits the rectangle in two parts
	if (diy) {
		auto endY = y - ny;
		if (endY >= 0) {
			return {Rect{Point{x, y}, Point{endX, endY}}};
		} else {
			return {Rect{Point{x, y}, Point{endX, 0}},
			        Rect{Point{x, height - 1},
			             Point{endX, endY & (height - 1)}}};
		}
	} else {
		auto endY = (y + ny);
		if (endY < height) {
			return {Rect{Point{x, y}, Point{endX, endY}}};
		} else {
			return {Rect{Point{x, y}, Point{endX, height - 1}},
			        Rect{Point{x, 0},
			             Point{endX, endY & (height - 1)}}};
		}
	}
}

// Given a VDPCmdEngine-rectangle and a point, split that rectangle in a 'done'
// and a 'todo' part.
//
// The VDPCmdEngine walks over this rectangle line by line. But the direction
// can still vary ('dix' and 'diy'). These directions are implicit: the first
// 'Point' in 'Rect' is the start point, the second is the end point. (Start and
// end points are inclusive).
//
// The given point is assumed to be 'done' (in reality we can make a small error
// here, but that's just a few dozen VDP cycles, so just a few Z80
// instructions). If the point lies outside the rectangle, the result will be
// shown as fully 'done'.
//
// The full result contains 1 or 2 'done' parts, and 0, 1 or 2 'todo' parts. So
// e.g. a top rectangle of fully processed lines, a middle line that's in
// progress (split over two lines (represented as a rectangle with height=1)),
// and a bottom rectangle of still todo lines.
//
// All the sub-rectangles in the result have their corners in 'natural' order:
// left-to-right and top-to-bottom (this simplifies drawing later). This is
// explicitly not the case for the input rectangle(s) (those are ordered
// according to 'dix' and 'diy').
//
// The order of the sub-rectangles in the output is no longer guaranteed to be
// in VDP command processing order, and for drawing the overlay that doesn't
// matter.
DoneTodo splitRect(const Rect& r, int x, int y)
{
	DoneTodo result;
	auto& done = result.done;
	auto& todo = result.todo;

	auto minX = std::min(r.p1.x, r.p2.x);
	auto maxX = std::max(r.p1.x, r.p2.x);
	auto minY = std::min(r.p1.y, r.p2.y);
	auto maxY = std::max(r.p1.y, r.p2.y);

	if ((x < minX) || (x > maxX) || (y < minY) || (y > maxY)) {
		// outside rect
		done.emplace_back(Point{minX, minY}, Point{maxX, maxY});
		return result;
	}

	bool diy = r.p1.y > r.p2.y;
	if (minY < y) {
		// top part (either done or todo), full lines
		auto& res = diy ? todo : done;
		res.emplace_back(Point{minX, minY}, Point{maxX, y - 1});
	}
	if (r.p1.x <= r.p2.x) {
		// left-to-right
		assert(r.p1.x <= x);
		done.emplace_back(Point{r.p1.x, y}, Point{x, y});
		if (x < r.p2.x) {
			todo.emplace_back(Point{x + 1, y}, Point{r.p2.x, y});
		}
	} else {
		// right to-left
		assert(x <= r.p1.x);
		done.emplace_back(Point{x, y}, Point{r.p1.x, y});
		if (r.p2.x < x) {
			todo.emplace_back(Point{r.p2.x, y}, Point{x - 1, y});
		}
	}

	if (y < maxY) {
		// bottom part (either done or todo), full lines
		auto& res = diy ? done : todo;
		res.emplace_back(Point{minX, y + 1}, Point{maxX, maxY});
	}

	return result;
}

ImGuiBitmapViewer::ImGuiBitmapViewer(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title("Bitmap viewer")
{
	if (index) {
		strAppend(title, " (", index + 1, ')');
	}
}

void ImGuiBitmapViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiBitmapViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiBitmapViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	ImGui::SetNextWindowSize({528, 620}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp || vdp->isMSX1VDP()) return;

		auto parseMode = [](DisplayMode mode) {
			auto base = mode.getBase();
			if (base == DisplayMode::GRAPHIC4) return SCR5;
			if (base == DisplayMode::GRAPHIC5) return SCR6;
			if (base == DisplayMode::GRAPHIC6) return SCR7;
			if (base != DisplayMode::GRAPHIC7) return OTHER;
			if (mode.getByte() & DisplayMode::YJK) {
				if (mode.getByte() & DisplayMode::YAE) {
					return SCR11;
				} else {
					return SCR12;
				}
			} else {
				return SCR8;
			}
		};
		int vdpMode = parseMode(vdp->getDisplayMode());

		int vdpPages = vdpMode <= SCR6 ? 4 : 2;
		int vdpPage = vdp->getDisplayPage();
		if (vdpPage >= vdpPages) vdpPage &= 1;

		int vdpLines = (vdp->getNumberOfLines() == 192) ? 0 : 1;

		int vdpColor0 = [&]{
			if (vdpMode == one_of(SCR8, SCR11, SCR12) || !vdp->getTransparency()) {
				return 16; // no replacement
			}
			return vdp->getBackgroundColor() & 15;
		}();

		auto modeToStr = [](int mode) {
			if (mode == SCR5 ) return "screen 5";
			if (mode == SCR6 ) return "screen 6";
			if (mode == SCR7 ) return "screen 7";
			if (mode == SCR8 ) return "screen 8";
			if (mode == SCR11) return "screen 11";
			if (mode == SCR12) return "screen 12";
			if (mode == OTHER) return "non-bitmap";
			assert(false); return "ERROR";
		};

		static const char* const color0Str = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";
		bool manualMode   = overrideAll || overrideMode;
		bool manualPage   = overrideAll || overridePage;
		bool manualLines  = overrideAll || overrideLines;
		bool manualColor0 = overrideAll || overrideColor0;
		im::Group([&]{
			ImGui::TextUnformatted("VDP settings");
			im::Disabled(manualMode, [&]{
				ImGui::AlignTextToFramePadding();
				ImGui::StrCat("Screen mode: ", modeToStr(vdpMode));
			});
			im::Disabled(manualPage, [&]{
				ImGui::AlignTextToFramePadding();
				ImGui::StrCat("Display page: ", vdpPage);
			});
			im::Disabled(manualLines, [&]{
				ImGui::AlignTextToFramePadding();
				ImGui::StrCat("Visible lines: ", vdpLines ? 212 : 192);
			});
			im::Disabled(manualColor0, [&]{
				ImGui::AlignTextToFramePadding();
				ImGui::StrCat("Replace color 0: ", getComboString(vdpColor0, color0Str));
			});
			// TODO interlace
		});
		ImGui::SameLine();
		im::Group([&]{
			ImGui::Checkbox("Manual override", &overrideAll);
			im::Group([&]{
				im::Disabled(overrideAll, [&]{
					ImGui::Checkbox("##mode",   overrideAll ? &overrideAll : &overrideMode);
					ImGui::Checkbox("##page",   overrideAll ? &overrideAll : &overridePage);
					ImGui::Checkbox("##lines",  overrideAll ? &overrideAll : &overrideLines);
					ImGui::Checkbox("##color0", overrideAll ? &overrideAll : &overrideColor0);
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
					im::Disabled(!manualMode, [&]{
						ImGui::Combo("##Screen mode", &bitmapScrnMode, "screen 5\000screen 6\000screen 7\000screen 8\000screen 11\000screen 12\000");
					});
					im::Disabled(!manualPage, [&]{
						int numPages = bitmapScrnMode <= SCR6 ? 4 : 2; // TODO extended VRAM
						if (bitmapPage >= numPages) bitmapPage = numPages - 1;
						if (bitmapPage < 0) bitmapPage = numPages;
						ImGui::Combo("##Display page", &bitmapPage, numPages == 2 ? "0\0001\000All\000" : "0\0001\0002\0003\000All\000");
						if (bitmapPage == numPages) bitmapPage = -1;
					});
					im::Disabled(!manualLines || bitmapPage < 0, [&]{
						ImGui::Combo("##Visible lines", &bitmapLines, "192\000212\000256\000");
					});
					im::Disabled(!manualColor0, [&]{
						ImGui::Combo("##Color 0 replacement", &bitmapColor0, color0Str);
					});
				});
			});
		});

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(15, 1));
		ImGui::SameLine();

		const auto& vram = vdp->getVRAM();
		int mode   = manualMode   ? bitmapScrnMode : vdpMode;
		int page   = manualPage   ? bitmapPage     : vdpPage;
		int lines  = manualLines  ? bitmapLines    : vdpLines;
		int color0 = manualColor0 ? bitmapColor0   : vdpColor0;
		int divX = mode == one_of(SCR6, SCR7) ? 1 : 2;
		int width  = 512 / divX;
		int height = (lines == 0) ? 192
		           : (lines == 1) ? 212
		           : 256;
		if (page < 0) {
			int numPages = mode <= SCR6 ? 4 : 2;
			height = 256 * numPages;
			page = 0;
		}
		auto rasterBeamPos = vdp->getMSXPos(vdp->getCurrentTime());
		rasterBeamPos.x /= divX;

		im::Group([&]{
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
			ImGui::Combo("Palette", &manager.palette->whichPalette, "VDP\000Custom\000Fixed\000");
			if (ImGui::Button("Open palette editor")) {
				manager.palette->window.raise();
			}
			ImGui::Separator();
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
			ImGui::Combo("Zoom", &bitmapZoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
			ImGui::Checkbox("grid", &bitmapGrid);
			ImGui::SameLine();
			im::Disabled(!bitmapGrid, [&]{
				ImGui::ColorEdit4("Grid color", bitmapGridColor.data(),
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
			});
			ImGui::Separator();
			ImGui::Checkbox("beam", &rasterBeam);
			ImGui::SameLine();
			im::Disabled(!rasterBeam, [&]{
				ImGui::ColorEdit4("raster beam color", rasterBeamColor.data(),
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				ImGui::SameLine();
				im::Font(manager.fontMono, [&]{
					ImGui::StrCat('(',  dec_string<4>(rasterBeamPos.x),
					              ',', dec_string<4>(rasterBeamPos.y), ')');
				});
			});
			HelpMarker("Position of the raster beam, expressed in MSX coordinates.\n"
			           "Left/top border have negative x/y-coordinates.\n"
			           "Only practically useful when emulation is paused.");
		});

		// VDP block command overlay settings
		auto& cmdEngine = vdp->getCmdEngine();
		bool inProgress = cmdEngine.commandInProgress(motherBoard->getCurrentTime());

		auto [sx, sy, dx, dy, nx, ny, col, arg, cmdReg] = cmdEngine.getLastCommand();
		auto cmd = cmdReg >> 4;
		bool dix = arg & VDPCmdEngine::DIX;
		bool diy = arg & VDPCmdEngine::DIY;
		bool byteMode = cmd >= 12; // hmmv, hmmm, ymmm, hmmc

		// only calculate src/dst rect when needed for the command
		std::optional<static_vector<Rect, 2>> srcRect;
		std::optional<static_vector<Rect, 2>> dstRect;
		if (cmd == one_of(9, 10, 13)) { // lmmm, lmcm, hmmm
			srcRect = rectFromVdpCmd(sx, sy, nx, ny, dix, diy, ScrnMode(mode), byteMode);
		}
		if (cmd == one_of(8, 9, 11, 12, 13, 15)) { // lmmv, lmmm, lmmc, hmmv, hmmm, hmmc
			dstRect = rectFromVdpCmd(dx, dy, nx, ny, dix, diy, ScrnMode(mode), byteMode);
		}
		if (cmd == 14) { // ymmm
			// different from normal: NO 'sx', and NO 'nx'
			srcRect = rectFromVdpCmd(dx, sy, 512, ny, dix, diy, ScrnMode(mode), byteMode);
			dstRect = rectFromVdpCmd(dx, dy, 512, ny, dix, diy, ScrnMode(mode), byteMode);
		}

		im::TreeNode("Show VDP block command overlay", [&]{
			ImGui::RadioButton("never", &showCmdOverlay, 0);
			HelpMarker("Don't draw any overlay in the bitmap view");
			ImGui::SameLine();
			ImGui::RadioButton("in progress", &showCmdOverlay, 1);
			HelpMarker("Only show overlay for VDP block command that is currently executing");
			ImGui::SameLine();
			ImGui::RadioButton("also finished", &showCmdOverlay, 2);
			HelpMarker("Show overlay for the currently executing VDP block command, but also the last finished block command");

			im::TreeNode("Overlay colors", [&]{
				int colorFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
				                 ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
				im::Group([&]{
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Source (done)"sv);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Source (todo)"sv);
				});
				ImGui::SameLine();
				im::Group([&]{
					ImGui::ColorEdit4("src-done", colorSrcDone.data(), colorFlags);
					ImGui::ColorEdit4("src-todo", colorSrcTodo.data(), colorFlags);
				});
				ImGui::SameLine(0.0f, ImGui::GetFontSize() * 3.0f);
				im::Group([&]{
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Destination (done)"sv);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Destination (todo)"sv);
				});
				ImGui::SameLine();
				im::Group([&]{
					ImGui::ColorEdit4("dst-done", colorDstDone.data(), colorFlags);
					ImGui::ColorEdit4("dst-todo", colorDstTodo.data(), colorFlags);
				});
			});

			ImGui::TextUnformatted("Last command "sv);
			ImGui::SameLine();
			ImGui::TextUnformatted(inProgress ? "[in progress]"sv : "[finished]"sv);

			// Textual-representation of last command (grayed-out if finished).
			// Note:
			// * The representation _resembles_ to notation in MSX-BASIC, but is (intentionally) not
			//   the same. Here we need to convey more information (e.g. for copy commands we show
			//   both corners of the destination).
			// * For the non-block commands we don't clamp/wrap the coordinates.
			// * For the logical commands we don't mask the color register (e.g. to 4 bits for SCRN5).
			// * For the LINE command, we don't clamp/wrap the end-point.
			// * Src and dst rectangles are _separately_ clamped to left/right borders.
			//   That's different from the real VDP behavior, but maybe less confusing to show in the debugger.
			// * The first coordinate is always the start-corner. The second may be larger or
			//   smaller depending on 'dix' and 'diy'.
			// * The textual representation for commands that wrap around the top/bottom border is
			//   ambiguous (but the drawn overlay should be fine).
			im::DisabledIndent(!inProgress, [&]{
				static constexpr std::array<const char*, 16> logOps = {
					"",     ",AND", ",OR", ",XOR", ",NOT", "","","",
					",TIMP",",TAND",",TOR",",TXOR",",TNOT","","",""
				};
				const char* logOp = logOps[cmdReg & 15];
				auto printRect = [](const char* s, std::optional<static_vector<Rect, 2>>& r) {
					assert(r);
					ImGui::TextUnformatted(s);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::Text("(%d,%d)-(%d,%d)",
						r->front().p1.x, // front/back for the (unlikely) case of vertical wrapping
						r->front().p1.y,
						r->back().p2.x,
						r->back().p2.y);
				};
				auto printSrcDstRect = [&](const char* c) {
					printRect(c, srcRect);
					ImGui::SameLine(0.0f, 0.0f);
					printRect(" TO ", dstRect);
				};
				auto printCol = [&]{
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::Text(",%d", col);
				};
				auto printLogOp = [&]{
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::TextUnformatted(logOp);
				};
				switch (cmd) {
				case 0: case 1: case 2: case 3:
					ImGui::TextUnformatted("ABORT"sv);
					break;
				case 4:
					ImGui::Text("POINT (%d,%d)", sx, sy);
					break;
				case 5:
					ImGui::Text("PSET (%d,%d),%d%s", dx, dy, col, logOp);
					break;
				case 6:
					ImGui::Text("SRCH (%d,%d) %s %d", sx, sy,
					            ((arg & VDPCmdEngine::EQ) ? "==" : "!="), col);
					break;
				case 7: {
					auto nx2 = nx; auto ny2 = ny;
					if (arg & VDPCmdEngine::MAJ) std::swap(nx2, ny2);
					auto x = int(sx) + (dix ? -int(nx2) : int(nx2));
					auto y = int(sy) + (diy ? -int(ny2) : int(ny2));
					ImGui::Text("LINE (%d,%d)-(%d,%d),%d%s", sx, sy, x, y, col, logOp);
					break;
				}
				case 8:
					printRect("LMMV ", dstRect);
					printCol();
					printLogOp();
					break;
				case 9:
					printSrcDstRect("LMMM ");
					printLogOp();
					break;
				case 10:
					printRect("LMCM ", srcRect);
					break;
				case 11:
					printRect("LMMC ", dstRect);
					printLogOp();
					break;
				case 12:
					printRect("HMMV ", dstRect);
					printCol();
					break;
				case 13:
					printSrcDstRect("HMMM ");
					break;
				case 14:
					printSrcDstRect("YMMM ");
					break;
				case 15:
					printRect("HMMC ", dstRect);
					break;
				default:
					UNREACHABLE;
				}
			});
		});

		ImGui::Separator();

		std::array<uint32_t, 16> palette;
		auto msxPalette = manager.palette->getPalette(vdp);
		ranges::transform(msxPalette, palette.data(),
			[](uint16_t msx) { return ImGuiPalette::toRGBA(msx); });
		if (color0 < 16) palette[0] = palette[color0];

		MemBuffer<uint32_t> pixels(512 * 256 * 4); // max size: screen 6/7, show all pages
		renderBitmap(vram.getData(), palette, mode, height, page,
				pixels.data());
		if (!bitmapTex) {
			bitmapTex.emplace(false, false); // no interpolation, no wrapping
		}
		bitmapTex->bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		int zx = (1 + bitmapZoom) * divX;
		int zy = (1 + bitmapZoom) * 2;
		auto zm = gl::vec2(float(zx), float(zy));

		gl::vec2 scrnPos;
		auto msxSize = gl::vec2(float(width), float(height));
		auto size = msxSize * zm;
		auto availSize = gl::vec2(ImGui::GetContentRegionAvail()) - gl::vec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
		auto reqSize = size + gl::vec2(ImGui::GetStyle().ScrollbarSize);
		im::Child("##bitmap", min(availSize, reqSize), 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
			scrnPos = ImGui::GetCursorScreenPos();
			auto pos = ImGui::GetCursorPos();
			ImGui::Image(bitmapTex->getImGui(), size);

			if (bitmapGrid && (zx > 1) && (zy > 1)) {
				auto color = ImGui::ColorConvertFloat4ToU32(bitmapGridColor);
				for (auto y : xrange(zy)) {
					auto* line = &pixels[y * zx];
					for (auto x : xrange(zx)) {
						line[x] = (x == 0 || y == 0) ? color : 0;
					}
				}
				if (!bitmapGridTex) {
					bitmapGridTex.emplace(false, true); // no interpolation, with wrapping
				}
				bitmapGridTex->bind();
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, zx, zy, 0,
						GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				ImGui::SetCursorPos(pos);
				ImGui::Image(bitmapGridTex->getImGui(), size, gl::vec2{}, msxSize);
			}
			auto* drawList = ImGui::GetWindowDrawList();
			if (rasterBeam) {
				auto center = scrnPos + (gl::vec2(rasterBeamPos) + gl::vec2{0.5f}) * zm;
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

			// draw the VDP command overlay
			auto offset = gl::vec2(0.0f, height > 256 ? 0.0f : float(256 * page));
			auto drawRect = [&](const Rect& r, ImU32 color) {
				auto tl = zm * (gl::vec2(r.p1) - offset);
				auto br = zm * (gl::vec2(r.p2) - offset + gl::vec2(1.0f)); // rect is inclusive, openGL is exclusive
				drawList->AddRectFilled(scrnPos + tl, scrnPos + br, color);
			};
			auto drawSplit = [&](const DoneTodo& dt, ImU32 colDone, ImU32 colTodo) {
				for (const auto& r : dt.done) drawRect(r, colDone);
				for (const auto& r : dt.todo) drawRect(r, colTodo);
			};
			auto drawOverlay = [&](const static_vector<Rect, 2>& rects, int x, int y, ImU32 colDone, ImU32 colTodo) {
				auto split1 = splitRect(rects[0], x, y);
				drawSplit(split1, colDone, colTodo);
				if (rects.size() == 2) {
					auto split2 = splitRect(rects[1], x, y);
					auto col1 = (split1.done.size() == 2) ? colTodo : colDone; // if (x,y) is in rects[0], then use todo-color
					drawSplit(split2, col1, colTodo);
				}
			};
			if ((showCmdOverlay == 2) || ((showCmdOverlay == 1) && inProgress)) {
				auto [sx2, sy2, dx2, dy2] = cmdEngine.getInprogressPosition();
				if (srcRect) {
					drawOverlay(*srcRect, sx2, sy2,
					            ImGui::ColorConvertFloat4ToU32(colorSrcDone),
					            ImGui::ColorConvertFloat4ToU32(colorSrcTodo));
				}
				if (dstRect) {
					drawOverlay(*dstRect, dx2, dy2,
					            ImGui::ColorConvertFloat4ToU32(colorDstDone),
					            ImGui::ColorConvertFloat4ToU32(colorDstTodo));
				}
			}
		});
		if (ImGui::IsItemHovered() && (mode != OTHER)) {
			auto [x_, y_] = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zm);
			auto x = x_; auto y = y_; // clang workaround
			if ((0 <= x) && (x < width) && (0 <= y) && (y < height)) {
				auto dec3 = [&](int d) {
					im::ScopedFont sf(manager.fontMono);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::Text("%3d", d);
				};
				auto hex2 = [&](unsigned h) {
					im::ScopedFont sf(manager.fontMono);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::StrCat(hex_string<2>(h));
				};
				auto hex5 = [&](unsigned h) {
					im::ScopedFont sf(manager.fontMono);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::StrCat(hex_string<5>(h));
				};

				ImGui::TextUnformatted("x="sv); dec3(x);
				ImGui::SameLine();
				ImGui::TextUnformatted("y="sv); dec3(y % 256);

				if (bitmapPage == -1) {
					ImGui::SameLine();
					ImGui::TextUnformatted("page="sv);
					im::ScopedFont sf(manager.fontMono);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::StrCat(y / 256);
				}

				unsigned physAddr = 0x8000 * page + 128 * y;
				switch (mode) {
				case SCR5: physAddr += x / 2; break;
				case SCR6: physAddr += x / 4; break;
				case SCR7: physAddr += x / 4 + 0x08000 * (x & 2); break;
				case SCR8: case SCR11: case SCR12:
						physAddr += x / 2 + 0x10000 * (x & 1); break;
				default: assert(false);
				}

				auto value = vram.getData()[physAddr];
				auto color = [&]() -> uint8_t {
					switch (mode) {
					case SCR5: case SCR7:
						return (value >> (4 * (1 - (x & 1)))) & 0x0f;
					case SCR6:
						return (value >> (2 * (3 - (x & 3)))) & 0x03;
					default:
						return value;
					}
				}();
				if (mode != one_of(SCR11, SCR12)) {
					ImGui::SameLine();
					ImGui::TextUnformatted("  color="sv); dec3(color);
				}

				ImGui::SameLine();
				ImGui::TextUnformatted("  vram: addr=0x");
				if (mode == one_of(SCR5, SCR6)) {
					hex5(physAddr);
				} else {
					unsigned logAddr = (physAddr & 0x0ffff) << 1 | (physAddr >> 16);
					hex5(logAddr),
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::TextUnformatted("(log)/0x"sv); hex5(physAddr);
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::TextUnformatted("(phys)"sv);
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(" value=0x"sv); hex2(value);
			}
		}
	});
}

// TODO avoid code duplication with src/video/BitmapConverter
void ImGuiBitmapViewer::renderBitmap(std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette16,
                                     int mode, int lines, int page, uint32_t* output) const
{
	auto yjk2rgb = [](int y, int j, int k) -> std::tuple<int, int, int> {
		// Note the formula for 'blue' differs from the 'traditional' formula
		// (e.g. as specified in the V9958 datasheet) in the rounding behavior.
		// Confirmed on real turbor machine. For details see:
		//    https://github.com/openMSX/openMSX/issues/1394
		//    https://twitter.com/mdpc___/status/1480432007180341251?s=20
		int r = std::clamp(y + j,                       0, 31);
		int g = std::clamp(y + k,                       0, 31);
		int b = std::clamp((5 * y - 2 * j - k + 2) / 4, 0, 31);
		return {r, g, b};
	};

	// TODO handle less than 128kB VRAM (will crash now)
	size_t addr = 0x8000 * page;
	switch (mode) {
	case SCR5:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(128)) {
				auto value = vram[addr];
				line[2 * x + 0] = palette16[(value >> 4) & 0x0f];
				line[2 * x + 1] = palette16[(value >> 0) & 0x0f];
				++addr;
			}
		}
		break;

	case SCR6:
		for (auto y : xrange(lines)) {
			auto* line = &output[512 * y];
			for (auto x : xrange(128)) {
				auto value = vram[addr];
				line[4 * x + 0] = palette16[(value >> 6) & 3];
				line[4 * x + 1] = palette16[(value >> 4) & 3];
				line[4 * x + 2] = palette16[(value >> 2) & 3];
				line[4 * x + 3] = palette16[(value >> 0) & 3];
				++addr;
			}
		}
		break;

	case SCR7:
		for (auto y : xrange(lines)) {
			auto* line = &output[512 * y];
			for (auto x : xrange(128)) {
				auto value0 = vram[addr + 0x00000];
				auto value1 = vram[addr + 0x10000];
				line[4 * x + 0] = palette16[(value0 >> 4) & 0x0f];
				line[4 * x + 1] = palette16[(value0 >> 0) & 0x0f];
				line[4 * x + 2] = palette16[(value1 >> 4) & 0x0f];
				line[4 * x + 3] = palette16[(value1 >> 0) & 0x0f];
				++addr;
			}
		}
		break;

	case SCR8: {
		auto toColor = [](uint8_t value) {
			int r = (value & 0x1c) >> 2;
			int g = (value & 0xe0) >> 5;
			int b = (value & 0x03) >> 0;
			int rr = (r << 5) | (r << 2) | (r >> 1);
			int gg = (g << 5) | (g << 2) | (g >> 1);
			int bb = (b << 6) | (b << 4) | (b << 2) | (b << 0);
			int aa = 255;
			return (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
		};
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(128)) {
				line[2 * x + 0] = toColor(vram[addr + 0x00000]);
				line[2 * x + 1] = toColor(vram[addr + 0x10000]);
				++addr;
			}
		}
		break;
	}

	case SCR11:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(64)) {
				std::array<unsigned, 4> p = {
					vram[addr + 0 + 0x00000],
					vram[addr + 0 + 0x10000],
					vram[addr + 1 + 0x00000],
					vram[addr + 1 + 0x10000],
				};
				addr += 2;
				int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
				int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);
				for (auto n : xrange(4)) {
					uint32_t pix;
					if (p[n] & 0x08) {
						pix = palette16[p[n] >> 4];
					} else {
						int Y = narrow<int>(p[n] >> 3);
						auto [r, g, b] = yjk2rgb(Y, j, k);
						int rr = (r << 3) | (r >> 2);
						int gg = (g << 3) | (g >> 2);
						int bb = (b << 3) | (b >> 2);
						int aa = 255;
						pix = (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
					}
					line[4 * x + n] = pix;
				}
			}
		}
		break;

	case SCR12:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(64)) {
				std::array<unsigned, 4> p = {
					vram[addr + 0 + 0x00000],
					vram[addr + 0 + 0x10000],
					vram[addr + 1 + 0x00000],
					vram[addr + 1 + 0x10000],
				};
				addr += 2;
				int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
				int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);
				for (auto n : xrange(4)) {
					int Y = narrow<int>(p[n] >> 3);
					auto [r, g, b] = yjk2rgb(Y, j, k);
					int rr = (r << 3) | (r >> 2);
					int gg = (g << 3) | (g >> 2);
					int bb = (b << 3) | (b >> 2);
					int aa = 255;
					line[4 * x + n] = (rr << 0) | (gg << 8) | (bb << 16) | (aa << 24);
				}
			}
		}
		break;

	case OTHER:
		for (auto y : xrange(lines)) {
			auto* line = &output[256 * y];
			for (auto x : xrange(256)) {
				line[x] = getColor(imColor::GRAY);
			}
		}
		break;
	}
}

} // namespace openmsx
