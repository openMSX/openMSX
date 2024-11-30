#ifndef IMGUI_BITMAP_VIEWER_HH
#define IMGUI_BITMAP_VIEWER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"
#include "static_vector.hh"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace openmsx {

class ImGuiBitmapViewer final : public ImGuiPart
{
public:
	enum ScrnMode : int { SCR5, SCR6, SCR7, SCR8, SCR11, SCR12, OTHER };

public:
	ImGuiBitmapViewer(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void renderBitmap(std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette16,
	                  int mode, int lines, int page, uint32_t* output) const;

public:
	bool show = true;

private:
	std::string title;

	int bitmapScrnMode = 0;
	int bitmapPage = 0; // 0-3 or 0-1 depending on screen mode, -1 for all pages   TODO extended VRAM
	int bitmapLines = 1; // 0->192, 1->212, 2->256
	int bitmapColor0 = 16; // 0..15, 16->no replacement
	int bitmapZoom = 0; // 0->1x, 1->2x, ..., 7->8x
	gl::vec4 bitmapGridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::vec4 rasterBeamColor{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	bool overrideAll    = false;
	bool overrideMode   = false;
	bool overridePage   = false;
	bool overrideLines  = false;
	bool overrideColor0 = false;
	bool bitmapGrid = true;
	bool rasterBeam = false;

	std::optional<gl::Texture> bitmapTex; // TODO also deallocate when needed
	std::optional<gl::Texture> bitmapGridTex;

	int showCmdOverlay = 0; // 0->none, 1->in-progress, 2->also finished
	gl::vec4 colorSrcDone{0.0f, 1.0f, 0.0f, 0.66f};
	gl::vec4 colorSrcTodo{0.0f, 1.0f, 0.0f, 0.33f};
	gl::vec4 colorDstDone{1.0f, 0.0f, 0.0f, 0.66f};
	gl::vec4 colorDstTodo{1.0f, 0.0f, 0.0f, 0.33f};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",           &ImGuiBitmapViewer::show},
		PersistentElement   {"overrideAll",    &ImGuiBitmapViewer::overrideAll},
		PersistentElement   {"overrideMode",   &ImGuiBitmapViewer::overrideMode},
		PersistentElement   {"overridePage",   &ImGuiBitmapViewer::overridePage},
		PersistentElement   {"overrideLines",  &ImGuiBitmapViewer::overrideLines},
		PersistentElement   {"overrideColor0", &ImGuiBitmapViewer::overrideColor0},
		PersistentElementMax{"scrnMode",       &ImGuiBitmapViewer::bitmapScrnMode, OTHER}, // SCR5..SCR12
		PersistentElement   {"page",           &ImGuiBitmapViewer::bitmapPage},
		PersistentElementMax{"lines",          &ImGuiBitmapViewer::bitmapLines, 3},
		PersistentElementMax{"color0",         &ImGuiBitmapViewer::bitmapColor0, 16 + 1},
		PersistentElementMax{"zoom",           &ImGuiBitmapViewer::bitmapZoom, 8},
		PersistentElement   {"showGrid",       &ImGuiBitmapViewer::bitmapGrid},
		PersistentElement   {"gridColor",      &ImGuiBitmapViewer::bitmapGridColor},
		PersistentElement   {"showRasterBeam", &ImGuiBitmapViewer::rasterBeam},
		PersistentElement   {"rasterBeamColor",&ImGuiBitmapViewer::rasterBeamColor},
		PersistentElementMax{"showCmdOverlay", &ImGuiBitmapViewer::showCmdOverlay, 3},
		PersistentElement   {"colorSrcDone",   &ImGuiBitmapViewer::colorSrcDone},
		PersistentElement   {"colorSrcTodo",   &ImGuiBitmapViewer::colorSrcTodo},
		PersistentElement   {"colorDstDone",   &ImGuiBitmapViewer::colorDstDone},
		PersistentElement   {"colorDstTodo",   &ImGuiBitmapViewer::colorDstTodo}
	};
};

using Point = gl::ivec2;
struct Rect {
	Rect() = default; // workaround
	Rect(const Point& p1_, const Point& p2_) : p1(p1_), p2(p2_) {} // workaround apple-clang-16(?) limitation
	Point p1, p2;
};
struct DoneTodo {
	static_vector<Rect, 2> done, todo;
};

// TODO write a unittest for these 2 functions.
static_vector<Rect, 2> rectFromVdpCmd(
	int x, int y, int nx, int ny,
	bool dix, bool diy, ImGuiBitmapViewer::ScrnMode screenMode, bool byteMode);
DoneTodo splitRect(const Rect& r, int x, int y);

} // namespace openmsx

#endif
