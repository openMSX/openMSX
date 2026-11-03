#ifndef IMGUI_V9990_BITMAP_HH
#define IMGUI_V9990_BITMAP_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

class V9990;

/** Bitmap viewer for the V9990 (GFX9000).
  *
  * This is the V9990 counterpart of ImGuiBitmapViewer. It shows the whole
  * VRAM as a bitmap: in the bitmap modes (B0-B7) using the image width and
  * color mode, in the pattern modes (P1, P2) the pattern data as it is
  * organized for the command engine.
  */
class ImGuiV9990Bitmap final : public ImGuiPart
{
public:
	ImGuiV9990Bitmap(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintP1(V9990& v9990, std::span<const uint32_t, 64> palette64,
	             uint8_t paletteOffset);
	void paintSingle(V9990& v9990, std::span<const uint32_t, 64> palette64,
	                 int mode, bool highRes, uint8_t paletteOffset,
	                 int width, int zx, int zy);
	void drawGrid(gl::vec2 pos, int zx, int zy, gl::vec2 msxSize, gl::vec2 size);

public:
	bool show = true;

private:
	std::string title;

	bool overrideAll        = false;
	bool overrideMode       = false;
	bool overrideColorMode  = false;
	bool overrideImageWidth = false;
	bool overridePaletteA   = false;
	bool overridePaletteB   = false;
	int manualMode = 0; // index in 'V9990_MODES' (P1, P2, B0..B4, B7)
	int manualColorMode = 1; // index in 'V9990_COLOR_MODES' (BP2, BP4, ...)
	int manualImageWidth = 1; // 0->256, 1->512, 2->1024, 3->2048
	int manualPaletteA = 0; // 0-3
	int manualPaletteB = 1; // 0-3

	int zoom = 0; // 0->1x, 1->2x, ..., 7->8x
	bool grid = true;
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA

	gl::Texture bitmapTex {gl::Null{}}; // TODO also deallocate when needed
	gl::Texture bitmapTexB{gl::Null{}}; // P1 layer B
	gl::Texture gridTex   {gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",               &ImGuiV9990Bitmap::show},
		PersistentElement   {"overrideAll",        &ImGuiV9990Bitmap::overrideAll},
		PersistentElement   {"overrideMode",       &ImGuiV9990Bitmap::overrideMode},
		PersistentElement   {"overrideColorMode",  &ImGuiV9990Bitmap::overrideColorMode},
		PersistentElement   {"overrideImageWidth", &ImGuiV9990Bitmap::overrideImageWidth},
		PersistentElement   {"overridePaletteA",   &ImGuiV9990Bitmap::overridePaletteA},
		PersistentElement   {"overridePaletteB",   &ImGuiV9990Bitmap::overridePaletteB},
		PersistentElementMax{"mode",               &ImGuiV9990Bitmap::manualMode, 8},
		PersistentElementMax{"colorMode",          &ImGuiV9990Bitmap::manualColorMode, 7},
		PersistentElementMax{"imageWidth",         &ImGuiV9990Bitmap::manualImageWidth, 4},
		PersistentElementMax{"paletteA",           &ImGuiV9990Bitmap::manualPaletteA, 4},
		PersistentElementMax{"paletteB",           &ImGuiV9990Bitmap::manualPaletteB, 4},
		PersistentElementMax{"zoom",               &ImGuiV9990Bitmap::zoom, 8},
		PersistentElement   {"showGrid",           &ImGuiV9990Bitmap::grid},
		PersistentElement   {"gridColor",          &ImGuiV9990Bitmap::gridColor}
	};
};

} // namespace openmsx

#endif
