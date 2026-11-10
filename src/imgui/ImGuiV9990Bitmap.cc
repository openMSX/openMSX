#include "ImGuiV9990Bitmap.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "ImGuiV9990Utils.hh"
#include "ImGuiVDPUtils.hh"

#include "MSXMotherBoard.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"

#include "MemBuffer.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <optional>

namespace openmsx {

using namespace std::literals;

ImGuiV9990Bitmap::ImGuiV9990Bitmap(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("V9990 bitmap viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiV9990Bitmap::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiV9990Bitmap::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// V9990 modes that can be selected with manual override, and the color modes
// for the bitmap (Bx) modes. B5 and B6 are not supported by the V9990.
static constexpr std::array V9990_MODES = {
	V9990DisplayMode::P1, V9990DisplayMode::P2,
	V9990DisplayMode::B0, V9990DisplayMode::B1, V9990DisplayMode::B2,
	V9990DisplayMode::B3, V9990DisplayMode::B4, V9990DisplayMode::B7,
};
static constexpr std::array V9990_COLOR_MODES = {
	V9990ColorMode::BP2, V9990ColorMode::BP4, V9990ColorMode::BP6, V9990ColorMode::BD8,
	V9990ColorMode::BYJK, V9990ColorMode::BYUV, V9990ColorMode::BD16,
};
static constexpr int MODE_P2 = -1; // 'mode' parameter for paintSingle(), others are V9990ColorMode values

[[nodiscard]] static int bitsPerPixel(int mode)
{
	if (mode == MODE_P2) return 4;
	switch (V9990ColorMode(mode)) {
		case V9990ColorMode::BP2:  return 2;
		case V9990ColorMode::BP4:  return 4;
		case V9990ColorMode::BD16: return 16;
		default:                   return 8;
	}
}

// Decoded info for one pixel in V9990 P2 or Bx mode.
struct V9990Pixel {
	unsigned addr;   // logical VRAM address (as used by the CPU and the command engine)
	unsigned value;  // raw VRAM value (8 bit, or 16 bit for BD16)
	int colorIndex;  // index in the 64 (or for BD8: 256) entry palette, -1 for direct colors
	uint32_t color;  // RGBA
};

// Mirrors the logic in V9990BitmapConverter and V9990PxConverter (P2).
[[nodiscard]] static V9990Pixel decodePixel(
	const V9990VRAM& vram, std::span<const uint32_t, 64> palette64,
	int mode, bool highRes, uint8_t offset, unsigned width, unsigned x, unsigned y)
{
	using v9990_gui::rgb5ToColor;
	auto pal64 = [&](unsigned addr, unsigned value, int idx) {
		return V9990Pixel{addr, value, idx, palette64[idx & 63]};
	};
	if (mode == MODE_P2) {
		// pattern data: 512 pixels wide, 4bpp, even/odd bytes use different palettes
		unsigned addr = 256 * y + x / 2;
		uint8_t value = vram.readVRAMBx(addr);
		int v = (x & 1) ? (value & 0x0f) : (value >> 4);
		int base = (addr & 1) ? ((offset & 0x0C) << 2) : ((offset & 0x03) << 4);
		return pal64(addr, value, base + v);
	}
	unsigned pixel = y * width + x;
	switch (V9990ColorMode(mode)) {
	case V9990ColorMode::BP2: {
		unsigned addr = pixel / 4;
		uint8_t value = vram.readVRAMBx(addr);
		int v = (value >> (2 * (3 - (x & 3)))) & 3;
		int idx = highRes ? (((offset & 7) << 2) + 32 * (x & 1) + v)
		                  : ((offset << 2) + v);
		return pal64(addr, value, idx);
	}
	case V9990ColorMode::BP4: {
		unsigned addr = pixel / 2;
		uint8_t value = vram.readVRAMBx(addr);
		int v = (x & 1) ? (value & 0x0f) : (value >> 4);
		int idx = highRes ? (((offset & 4) << 2) + 32 * (x & 1) + v)
		                  : (((offset & 0x0C) << 2) + v);
		return pal64(addr, value, idx);
	}
	case V9990ColorMode::BP6: {
		uint8_t value = vram.readVRAMBx(pixel);
		return pal64(pixel, value, value & 0x3f);
	}
	case V9990ColorMode::BD8: {
		static constexpr std::array<int, 8> mapRG = {0, 4, 9, 13, 18, 22, 27, 31};
		static constexpr std::array<int, 4> mapB = {0, 11, 21, 31};
		uint8_t value = vram.readVRAMBx(pixel);
		auto color = rgb5ToColor(mapRG[(value >> 2) & 7], mapRG[value >> 5], mapB[value & 3]);
		return {pixel, value, value, color};
	}
	case V9990ColorMode::BYJK:
	case V9990ColorMode::BYUV: {
		unsigned base = pixel & ~3;
		std::array<uint8_t, 4> data;
		for (auto i : xrange(4)) data[i] = vram.readVRAMBx(base + i);
		int u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		int v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);
		uint8_t value = data[x & 3];
		int yy = value >> 3;
		int r = std::clamp(yy + u,                    0, 31);
		int g = std::clamp((5 * yy - 2 * u - v) / 4, 0, 31);
		int b = std::clamp(yy + v,                    0, 31);
		if (V9990ColorMode(mode) == V9990ColorMode::BYJK) std::swap(g, b);
		return {base + (x & 3), value, -1, rgb5ToColor(r, g, b)};
	}
	case V9990ColorMode::BD16: {
		unsigned addr = 2 * pixel;
		unsigned value = vram.readVRAMBx(addr) + 256 * vram.readVRAMBx(addr + 1);
		return {addr, value, -1, rgb5ToColor(value >> 5, value >> 10, value)};
	}
	default:
		UNREACHABLE;
	}
}

void ImGuiV9990Bitmap::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({528, 620}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* v9990 = dynamic_cast<V9990*>(motherBoard->findDevice("Sunrise GFX9000")); // TODO name based OK?
		if (!v9990) return;
		using enum V9990DisplayMode;
		using v9990_gui::paletteStr;
		using v9990_gui::toString;

		auto vdpMode = v9990->getDisplayMode();
		auto vdpColorMode = v9990->getColorMode();
		unsigned vdpImageWidth = v9990->getImageWidth();
		uint8_t vdpPaletteOffset = v9990->getPaletteOffset();
		int vdpPaletteA = (vdpPaletteOffset >> 0) & 3;
		int vdpPaletteB = (vdpPaletteOffset >> 2) & 3;
		bool vdpIsPattern = vdpMode == one_of(P1, P2);

		bool manMode       = overrideAll || overrideMode;
		bool manColorMode  = overrideAll || overrideColorMode;
		bool manImageWidth = overrideAll || overrideImageWidth;
		bool manPalA       = overrideAll || overridePaletteA;
		bool manPalB       = overrideAll || overridePaletteB;

		auto mode = manMode ? V9990_MODES[manualMode] : vdpMode;
		auto colorMode = manColorMode ? V9990_COLOR_MODES[manualColorMode] : vdpColorMode;
		unsigned imageWidth = manImageWidth ? (256u << manualImageWidth) : vdpImageWidth;
		int palA = manPalA ? manualPaletteA : vdpPaletteA;
		int palB = manPalB ? manualPaletteB : vdpPaletteB;
		auto paletteOffset = uint8_t(palA | (palB << 2));
		bool isPattern = mode == one_of(P1, P2);

		static const char* const modeStr = "P1\000P2\000B0\000B1\000B2\000B3\000B4\000B7\000";
		static const char* const colorModeStr = "BP2\000BP4\000BP6\000BD8\000BYJK\000BYUV\000BD16\000";
		static const char* const widthStr = "256\000512\0001024\0002048\000";
		overridableSettings("V9990 settings", "Palette A: 0 (0-15)", overrideAll, {
			{tmpStrCat("Display mode: ", toString(vdpMode)), &overrideMode, [&]{
				ImGui::Combo("##mode", &manualMode, modeStr);
			}},
			{tmpStrCat("Color mode: ", vdpIsPattern ? "-" : toString(vdpColorMode)), &overrideColorMode, [&]{
				im::Disabled(isPattern, [&]{
					ImGui::Combo("##colorMode", &manualColorMode, colorModeStr);
				});
			}, !isPattern},
			{tmpStrCat("Image width: ", vdpImageWidth), &overrideImageWidth, [&]{
				im::Disabled(isPattern, [&]{
					ImGui::Combo("##imageWidth", &manualImageWidth, widthStr);
				});
			}, !isPattern},
			{tmpStrCat("Palette A: ", getComboString(vdpPaletteA, paletteStr)), &overridePaletteA, [&]{
				ImGui::Combo("##paletteA", &manualPaletteA, paletteStr);
			}},
			{tmpStrCat("Palette B: ", getComboString(vdpPaletteB, paletteStr)), &overridePaletteB, [&]{
				ImGui::Combo("##paletteB", &manualPaletteB, paletteStr);
			}},
		}, "Interpret VRAM using a manually selected mode, color mode, image width and/or palette offset.\n"
		   "\n"
		   "Palette A and B are the two halves of the palette offset register (R#13):\n"
		   "* P1: palette of layer A and layer B\n"
		   "* P2: palette of the even and odd bytes\n"
		   "* BP4: only palette B is used (in B4/B7 only bit 2)\n"
		   "* BP2: A and B together select the palette offset (in B4/B7 only bits 0-2)\n"
		   "* other color modes don't use the palette offset");

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(15, 1));
		ImGui::SameLine();

		im::Group([&]{
			zoomAndGridSettings(zoom, grid, gridColor);
		});

		ImGui::Separator();

		auto palette64 = v9990_gui::getPalette64(*v9990);

		// Aspect ratio: low-res modes have 256 (wide) pixels per line, high-res
		// modes 512 or more. B4 and B7 are typically used interlaced.
		bool highRes = mode == one_of(B4, B5, B6, B7);
		int zx = (1 + zoom) * ((mode == one_of(P1, B0, B1)) ? 2 : 1);
		int zy = (1 + zoom) * (highRes ? 1 : 2);

		if (mode == P1) {
			paintP1(*v9990, palette64, paletteOffset);
		} else if (mode == P2) {
			paintSingle(*v9990, palette64, MODE_P2, false, paletteOffset, 512, zx, zy);
		} else {
			paintSingle(*v9990, palette64, int(colorMode), highRes, paletteOffset,
			            narrow<int>(imageWidth), zx, zy);
		}
	});
}

// Show the whole VRAM as a single bitmap, for P2 and the bitmap (Bx) modes. In
// P2 mode the pattern data is organized as a 512-pixel wide, 4bpp bitmap. For
// Bx modes the width is the 'image width' (not the display width).
void ImGuiV9990Bitmap::paintSingle(
	V9990& v9990, std::span<const uint32_t, 64> palette64,
	int mode, bool highRes, uint8_t paletteOffset, int width, int zx, int zy)
{
	static const int maxTextureSize = []{
		GLint result = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &result);
		return int(result);
	}();
	int bpp = bitsPerPixel(mode);
	int height = std::min(narrow<int>(V9990VRAM::VRAM_SIZE * 8 / (width * bpp)), maxTextureSize);

	const auto& vram = v9990.getVRAM();
	MemBuffer<uint32_t> pixels(width * height);
	for (auto y : xrange(height)) {
		auto* line = &pixels[width * y];
		for (auto x : xrange(width)) {
			line[x] = decodePixel(vram, palette64, mode, highRes, paletteOffset,
			                      width, x, y).color;
		}
	}
	updateTexture(bitmapTex, {width, height}, pixels);

	auto zm = gl::vec2(float(zx), float(zy));
	auto msxSize = gl::vec2(float(width), float(height));
	auto size = msxSize * zm;
	auto availSize = gl::vec2(ImGui::GetContentRegionAvail()) - gl::vec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
	auto reqSize = size + gl::vec2(ImGui::GetStyle().ScrollbarSize);
	const gl::Texture* gridOverlay = nullptr;
	if (grid && (zx > 1) && (zy > 1)) {
		updateGridTexture(gridTex, {zx, zy}, ImGui::ColorConvertFloat4ToU32(gridColor));
		gridOverlay = &gridTex;
	}
	gl::vec2 scrnPos;
	im::Child("##bitmap", min(availSize, reqSize), 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
		scrnPos = ImGui::GetCursorScreenPos();
		imageWithGrid(bitmapTex, size, {}, {1.0f, 1.0f}, gridOverlay, msxSize);
	});
	if (ImGui::IsItemHovered()) {
		auto [x, y] = hoveredCell(scrnPos, zm);
		if ((0 <= x) && (x < width) && (0 <= y) && (y < height)) {
			auto pix = decodePixel(vram, palette64, mode, highRes, paletteOffset, width, x, y);
			auto mono = [&](const auto& s) {
				im::ScopedFont sf(manager.fontMono);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::StrCat(s);
			};
			ImGui::TextUnformatted("x="sv); mono(dec_string<4>(x));
			ImGui::SameLine();
			ImGui::TextUnformatted("y="sv); mono(dec_string<4>(y));
			if (mode == MODE_P2) {
				ImGui::SameLine();
				ImGui::TextUnformatted("  pattern="sv); mono(dec_string<4>(64 * (y / 8) + (x / 8)));
			}
			if (pix.colorIndex >= 0) {
				ImGui::SameLine();
				ImGui::TextUnformatted("  color="sv); mono(dec_string<3>(pix.colorIndex));
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("  vram: addr=0x"sv); mono(hex_string<5>(pix.addr));
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextUnformatted("(log)/0x"sv); mono(hex_string<5>(V9990VRAM::transformBx(pix.addr)));
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::TextUnformatted("(phys)"sv);
			ImGui::SameLine();
			ImGui::TextUnformatted(" value=0x"sv);
			if (bpp == 16) {
				mono(hex_string<4>(pix.value));
			} else {
				mono(hex_string<2>(pix.value));
			}
		}
	}
}

// Show the V9990 P1 pattern data as a bitmap. In P1 mode the pattern data of
// each layer is organized as a 256-pixel wide, 4bpp bitmap (128 bytes per
// line, 2048 lines): layer A starts at VRAM address 0x00000, layer B at
// 0x40000. This is also the coordinate system the V9990 command engine uses
// in P1 mode.
void ImGuiV9990Bitmap::paintP1(
	V9990& v9990, std::span<const uint32_t, 64> palette64, uint8_t paletteOffset)
{
	int palA = (paletteOffset >> 0) & 3;
	int palB = (paletteOffset >> 2) & 3;

	// Both layers are always shown side-by-side, each in its own scrollable
	// region showing the full 2048 lines.
	static constexpr int LAYER_WIDTH = 256;
	static constexpr int LAYER_LINES = 2048;
	static constexpr unsigned LAYER_B_BASE = 0x40000;

	const auto& vram = v9990.getVRAM();
	MemBuffer<uint32_t> pixels(LAYER_WIDTH * LAYER_LINES);
	auto uploadLayer = [&](int layer, gl::Texture& tex) {
		auto palette16 = subspan<16>(palette64, 16 * (layer ? palB : palA));
		unsigned addr = layer ? LAYER_B_BASE : 0;
		for (auto y : xrange(LAYER_LINES)) {
			auto* out = &pixels[LAYER_WIDTH * y];
			for (auto x : xrange(LAYER_WIDTH / 2)) {
				auto value = vram.readVRAMDirect(addr++);
				out[2 * x + 0] = palette16[value >> 4];
				out[2 * x + 1] = palette16[value & 0x0f];
			}
		}
		updateTexture(tex, {LAYER_WIDTH, LAYER_LINES}, pixels);
	};
	uploadLayer(0, bitmapTex);
	uploadLayer(1, bitmapTexB);

	int zx = (1 + zoom) * 2;
	int zy = (1 + zoom) * 2;
	auto zm = gl::vec2(float(zx), float(zy));
	auto msxSize = gl::vec2(float(LAYER_WIDTH), float(LAYER_LINES));
	auto size = msxSize * zm;

	auto spacing = ImGui::GetStyle().ItemSpacing.x;
	auto availSize = gl::vec2(ImGui::GetContentRegionAvail())
	               - gl::vec2(spacing, ImGui::GetTextLineHeightWithSpacing() * 2.0f);
	availSize.x *= 0.5f;
	auto reqSize = size + gl::vec2(ImGui::GetStyle().ScrollbarSize);
	auto childSize = min(availSize, reqSize);

	const gl::Texture* gridOverlay = nullptr;
	if (grid) { // zx and zy are at least 2 in P1
		updateGridTexture(gridTex, {zx, zy}, ImGui::ColorConvertFloat4ToU32(gridColor));
		gridOverlay = &gridTex;
	}
	std::optional<int> hoveredLayer;
	gl::ivec2 hoveredPos;
	auto drawLayer = [&](int layer, const gl::Texture& tex) {
		im::Group([&]{
			ImGui::StrCat("Layer ", layer ? 'B' : 'A',
			              "  (palette ", layer ? palB : palA, ')');
			gl::vec2 scrnPos;
			im::Child(layer ? "##layerB" : "##layerA", childSize, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				scrnPos = ImGui::GetCursorScreenPos();
				imageWithGrid(tex, size, {}, {1.0f, 1.0f}, gridOverlay, msxSize);
			});
			if (ImGui::IsItemHovered()) {
				hoveredLayer = layer;
				hoveredPos = hoveredCell(scrnPos, zm);
			}
		});
	};
	drawLayer(0, bitmapTex);
	ImGui::SameLine();
	drawLayer(1, bitmapTexB);

	if (hoveredLayer) {
		int layer = *hoveredLayer;
		auto [x, y] = hoveredPos;
		if ((0 <= x) && (x < LAYER_WIDTH) && (0 <= y) && (y < LAYER_LINES)) {
			unsigned addr = (layer ? LAYER_B_BASE : 0) + 128 * y + x / 2;
			auto value = vram.readVRAMDirect(addr);
			int color = (x & 1) ? (value & 0x0f) : (value >> 4);
			int pattern = 32 * (y / 8) + (x / 8);

			auto mono = [&](const auto& s) {
				im::ScopedFont sf(manager.fontMono);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::StrCat(s);
			};
			ImGui::StrCat("layer ", layer ? 'B' : 'A', "  x="); mono(dec_string<3>(x));
			ImGui::SameLine();
			ImGui::TextUnformatted("y="sv); mono(dec_string<4>(y));
			ImGui::SameLine();
			ImGui::TextUnformatted("  pattern="sv); mono(dec_string<4>(pattern));
			ImGui::SameLine();
			ImGui::TextUnformatted("  color="sv);
			mono(dec_string<2>(16 * (layer ? palB : palA) + color));
			ImGui::SameLine();
			ImGui::TextUnformatted("  vram: addr=0x"sv); mono(hex_string<5>(addr));
			ImGui::SameLine();
			ImGui::TextUnformatted(" value=0x"sv); mono(hex_string<2>(value));
		}
	}
}

} // namespace openmsx
