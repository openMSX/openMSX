#ifndef DEBUGGABLE_EDITOR_HH
#define DEBUGGABLE_EDITOR_HH

// Based on:
//     Mini memory editor for Dear ImGui (to embed in your game/tools)
//
//     Get latest version at http://www.github.com/ocornut/imgui_club
//     (See there for documentation and the (old) Changelog).
//
//     Licence: MIT

#include <imgui.h>

#include <string>

namespace openmsx {

class Debuggable;
class ImGuiManager;
class SymbolManager;

class DebuggableEditor
{
	struct Sizes {
		int addrDigitsCount = 0;
		float lineHeight = 0.0f;
		float glyphWidth = 0.0f;
		float hexCellWidth = 0.0f;
		float spacingBetweenMidCols = 0.0f;
		float posHexStart = 0.0f;
		float posAsciiStart = 0.0f;
		float posAsciiEnd = 0.0f;
		float windowWidth = 0.0f;
	};

public:
	explicit DebuggableEditor(ImGuiManager& manager_);
	void paint(const char* title, Debuggable& debuggable);

	bool open = true;

private:
	[[nodiscard]] Sizes calcSizes(unsigned memSize);
	void drawContents(const Sizes& s, Debuggable& debuggable, unsigned memSize);
	void drawPreviewLine(const Sizes& s, Debuggable& debuggable, unsigned memSize);

private:
	ImGuiManager* manager;
	SymbolManager* symbolManager;

	// Settings
	int  columns = 16;            // number of columns to display.
	bool showAscii = true;        // display ASCII representation on the right side.
	bool showAddress = true;      // display the address bar (e.g. on small views it can make sense to hide this)
	bool showDataPreview = false; // display a footer previewing the decimal/binary/hex/float representation of the currently selected bytes.
	bool greyOutZeroes = true;    // display null/zero bytes using the TextDisabled color.

	// [Internal State]
	enum AddressMode : int {CURSOR, EXPRESSION};
	enum PreviewEndianess : int {LE, BE};

	std::string dataInput;
	std::string addrStr;
	std::string addrExpr;
	unsigned currentAddr = 0;
	int addrMode = CURSOR;
	int previewEndianess = LE;
	ImGuiDataType previewDataType = ImGuiDataType_U8;
	bool dataEditingTakeFocus = true;
};

} // namespace openmsx

#endif
