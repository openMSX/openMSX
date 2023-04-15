#ifndef IMGUI_CHARACTER_HH
#define IMGUI_CHARACTER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <optional>

namespace openmsx {

class ImGuiManager;

class ImGuiCharacter final : public ImGuiPart
{
public:
	ImGuiCharacter(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "Tile viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	static void renderPatterns(int mode, std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette,
	                           int fgCol, int bgCol, int fgBlink, int bgBlink,
	                           int patBase, int colBase, int lines, std::span<uint32_t> pixels);

public:
	bool show = false;

private:
	ImGuiManager& manager;

	int manual = 0;
	int zoom = 0; // 0->1x, 1->2x, ..., 7->8x
	bool grid = true;
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA

	enum CharScrnMode : int { TEXT40, TEXT80, SCR1, SCR2, SCR3, SCR4, OTHER };
	int manualMode = 0;
	int manualFgCol = 15;
	int manualBgCol = 4;
	int manualFgBlink = 14;
	int manualBgBlink = 1;
	int manualBlink = 1;
	int manualPatBase = 0;
	int manualColBase = 0;
	int manualNamBase = 0;
	int manualRows = 0;
	int manualColor0 = 16;

	gl::Texture patternTex{gl::Null{}}; // TODO also deallocate when needed
	gl::Texture gridTex   {gl::Null{}};
};

} // namespace openmsx

#endif
