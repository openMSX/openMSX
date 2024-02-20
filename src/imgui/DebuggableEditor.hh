#ifndef DEBUGGABLE_EDITOR_HH
#define DEBUGGABLE_EDITOR_HH

// Based on:
//     Mini memory editor for Dear ImGui (to embed in your game/tools)
//
//     Get latest version at http://www.github.com/ocornut/imgui_club
//     (See there for documentation and the (old) Changelog).
//
//     Licence: MIT

#include "ImGuiPart.hh"

#include <imgui.h>

#include <string>

namespace openmsx {

class Debuggable;
class ImGuiManager;
class SymbolManager;

class DebuggableEditor : public ImGuiPart
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
	explicit DebuggableEditor(ImGuiManager& manager_, std::string debuggableName, size_t index);
	[[nodiscard]] std::string_view getDebuggableName() const { return {title.data(), debuggableNameSize}; }

	// ImGuiPart
	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool open = true;

private:
	[[nodiscard]] Sizes calcSizes(unsigned memSize);
	void drawContents(const Sizes& s, Debuggable& debuggable, unsigned memSize);
	void drawPreviewLine(const Sizes& s, Debuggable& debuggable, unsigned memSize);

private:
	enum AddressMode : int {CURSOR, EXPRESSION};
	enum PreviewEndianess : int {LE, BE};

	SymbolManager& symbolManager;
	std::string title; // debuggableName + suffix
	size_t debuggableNameSize;

	// Settings
	int  columns = 16;            // number of columns to display.
	bool showAscii = true;        // display ASCII representation on the right side.
	bool showAddress = true;      // display the address bar (e.g. on small views it can make sense to hide this)
	bool showDataPreview = false; // display a footer previewing the decimal/binary/hex/float representation of the currently selected bytes.
	bool greyOutZeroes = true;    // display null/zero bytes using the TextDisabled color.
	std::string addrExpr;
	unsigned currentAddr = 0;
	int addrMode = CURSOR;
	int previewEndianess = LE;
	ImGuiDataType previewDataType = ImGuiDataType_U8;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"open",             &DebuggableEditor::open},
		PersistentElementMax{"columns",          &DebuggableEditor::columns, 16+1},
		PersistentElement   {"showAscii",        &DebuggableEditor::showAscii},
		PersistentElement   {"showAddress",      &DebuggableEditor::showAddress},
		PersistentElement   {"showDataPreview",  &DebuggableEditor::showDataPreview},
		PersistentElement   {"greyOutZeroes",    &DebuggableEditor::greyOutZeroes},
		PersistentElement   {"addrExpr",         &DebuggableEditor::addrExpr},
		PersistentElement   {"currentAddr",      &DebuggableEditor::currentAddr},
		PersistentElementMax{"addrMode",         &DebuggableEditor::addrMode, 2},
		PersistentElementMax{"previewEndianess", &DebuggableEditor::previewEndianess, 2},
		PersistentElementMax{"previewDataType",  &DebuggableEditor::previewDataType, 8}
	};

	// [Internal State]
	std::string dataInput;
	std::string addrStr;
	bool dataEditingTakeFocus = true;
	bool dataEditingActive = false;
	bool updateAddr = false;
};

} // namespace openmsx

#endif
