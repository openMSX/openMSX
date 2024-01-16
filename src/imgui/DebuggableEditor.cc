#include "DebuggableEditor.hh"

#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Debuggable.hh"

#include "unreachable.hh"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <span>

namespace openmsx {

using namespace std::literals;

static constexpr int MidColsCount = 8; // extra spacing between every mid-cols.
static constexpr auto HighlightColor = IM_COL32(255, 255, 255, 50); // background color of highlighted bytes.

DebuggableEditor::Sizes DebuggableEditor::calcSizes(unsigned memSize)
{
	Sizes s;
	const auto& style = ImGui::GetStyle();

	s.addrDigitsCount = 0;
	for (unsigned n = memSize - 1; n > 0; n >>= 4) {
		++s.addrDigitsCount;
	}

	s.lineHeight = ImGui::GetTextLineHeight();
	s.glyphWidth = ImGui::CalcTextSize("F").x + 1;            // We assume the font is mono-space
	s.hexCellWidth = truncf(s.glyphWidth * 2.5f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
	s.spacingBetweenMidCols = truncf(s.hexCellWidth * 0.25f); // Every OptMidColsCount columns we add a bit of extra spacing
	s.posHexStart = float(s.addrDigitsCount + 2) * s.glyphWidth;
	s.posHexEnd = s.posHexStart + (s.hexCellWidth * float(columns));
	s.posAsciiStart = s.posAsciiEnd = s.posHexEnd;
	if (showAscii) {
		s.posAsciiStart = s.posHexEnd + s.glyphWidth * 1;
		int numMacroColumns = (columns + MidColsCount - 1) / MidColsCount;
		s.posAsciiStart += float(numMacroColumns) * s.spacingBetweenMidCols;
		s.posAsciiEnd = s.posAsciiStart + float(columns) * s.glyphWidth;
	}
	s.windowWidth = s.posAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.glyphWidth;
	return s;
}

void DebuggableEditor::paint(const char* title, Debuggable& debuggable)
{
	im::ScopedFont sf(manager->fontMono);

	unsigned memSize = debuggable.getSize();
	auto s = calcSizes(memSize);
	ImGui::SetNextWindowSize(ImVec2(s.windowWidth, s.windowWidth * 0.60f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(s.windowWidth, FLT_MAX));

	open = true;
	if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoScrollbar)) {
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("context");
		}
		drawContents(s, debuggable, memSize);
		if (contentsWidthChanged) {
			s = calcSizes(memSize);
			ImGui::SetWindowSize(ImVec2(s.windowWidth, ImGui::GetWindowSize().y));
		}
	}
	ImGui::End();
}

[[nodiscard]] static unsigned DataTypeGetSize(ImGuiDataType data_type)
{
	std::array<unsigned, ImGuiDataType_COUNT - 2> sizes = { 1, 1, 2, 2, 4, 4, 8, 8 };
	assert(data_type >= 0 && data_type < (ImGuiDataType_COUNT - 2));
	return sizes[data_type];
}

void DebuggableEditor::drawContents(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	columns = std::max(columns, 1);

	const auto& style = ImGui::GetStyle();

	// We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
	// This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.
	const auto height_separator = style.ItemSpacing.y;
	float footer_height = height_separator + ImGui::GetFrameHeightWithSpacing();
	if (showDataPreview) {
		footer_height += height_separator + ImGui::GetFrameHeightWithSpacing() + 3 * ImGui::GetTextLineHeightWithSpacing();
	}
	ImGui::BeginChild("##scrolling", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
	auto* draw_list = ImGui::GetWindowDrawList();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	// We are not really using the clipper API correctly here, because we rely on visible_start_addr/visible_end_addr for our scrolling function.
	const int line_total_count = int((memSize + columns - 1) / columns);
	ImGuiListClipper clipper;
	clipper.Begin(line_total_count, s.lineHeight);

	bool data_next = false;

	currentAddr = std::min(currentAddr, memSize - 1);

	std::optional<unsigned> nextAddr;
	// Move cursor but only apply on next frame so scrolling with be synchronized (because currently we can't change the scrolling while the window is being rendered)
	if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) &&
		ptrdiff_t(currentAddr) >= ptrdiff_t(columns)) {
		nextAddr = currentAddr - columns;
	} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) &&
			ptrdiff_t(currentAddr) < ptrdiff_t(memSize - columns)) {
		nextAddr = currentAddr + columns;
	} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) &&
			ptrdiff_t(currentAddr) > ptrdiff_t(0)) {
		nextAddr = currentAddr - 1;
	} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) &&
			ptrdiff_t(currentAddr) < ptrdiff_t(memSize - 1)) {
		nextAddr = currentAddr + 1;
	}

	// Draw vertical separator
	ImVec2 window_pos = ImGui::GetWindowPos();
	if (showAscii) {
		draw_list->AddLine(ImVec2(window_pos.x + s.posAsciiStart - s.glyphWidth, window_pos.y),
		                   ImVec2(window_pos.x + s.posAsciiStart - s.glyphWidth, window_pos.y + 9999),
		                   ImGui::GetColorU32(ImGuiCol_Border));
	}

	const auto color_text = ImGui::GetColorU32(ImGuiCol_Text);
	const auto color_disabled = greyOutZeroes ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : color_text;

	while (clipper.Step()) {
		for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; ++line_i) {
			auto addr = unsigned(line_i) * columns;
			ImGui::StrCat(hex_string<HexCase::upper>(Digits{size_t(s.addrDigitsCount)}, addr), ": ");

			// Draw Hexadecimal
			for (int n = 0; n < columns && addr < memSize; ++n, ++addr) {
				float byte_pos_x = s.posHexStart + s.hexCellWidth * float(n);
				int macroColumn = n / MidColsCount;
				byte_pos_x += float(macroColumn) * s.spacingBetweenMidCols;
				ImGui::SameLine(byte_pos_x);

				// Draw highlight
				auto preview_data_type_size = DataTypeGetSize(previewDataType);
				auto highLight = [&](unsigned a) {
					return (currentAddr <= a) && (a < (currentAddr + preview_data_type_size));
				};
				if (highLight(addr)) {
					ImVec2 pos = ImGui::GetCursorScreenPos();
					float highlight_width = s.glyphWidth * 2;
					if (highLight(addr + 1)) {
						highlight_width = s.hexCellWidth;
						if (n > 0 && (n + 1) < columns && ((n + 1) % MidColsCount) == 0) {
							highlight_width += s.spacingBetweenMidCols;
						}
					}
					draw_list->AddRectFilled(pos, ImVec2(pos.x + highlight_width, pos.y + s.lineHeight), HighlightColor);
				}

				if (currentAddr == addr) {
					// Display text input on current byte
					bool data_write = false;
					ImGui::PushID(reinterpret_cast<void*>(addr));
					if (dataEditingTakeFocus) {
						ImGui::SetKeyboardFocusHere(0);
						sprintf(addrInputBuf.data(), "%0*X", s.addrDigitsCount, addr);
						sprintf(dataInputBuf.data(), "%02X", debuggable.read(addr));
					}
					struct UserData {
						// FIXME: We should have a way to retrieve the text edit cursor position more easily in the API, this is rather tedious. This is such a ugly mess we may be better off not using InputText() at all here.
						static int Callback(ImGuiInputTextCallbackData* data) {
							auto* user_data = static_cast<UserData*>(data->UserData);
							if (!data->HasSelection()) {
								user_data->CursorPos = data->CursorPos;
							}
							if (data->SelectionStart == 0 && data->SelectionEnd == data->BufTextLen) {
								// When not editing a byte, always refresh its InputText content pulled from underlying memory data
								// (this is a bit tricky, since InputText technically "owns" the master copy of the buffer we edit it in there)
								data->DeleteChars(0, data->BufTextLen);
								data->InsertChars(0, user_data->CurrentBufOverwrite.data());
								data->SelectionStart = 0;
								data->SelectionEnd = 2;
								data->CursorPos = 0;
							}
							return 0;
						}
						std::array<char, 3> CurrentBufOverwrite = {}; // Input
						int CursorPos = -1; // Output
					};
					UserData user_data;
					sprintf(user_data.CurrentBufOverwrite.data(), "%02X", debuggable.read(addr));
					ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal
					                          | ImGuiInputTextFlags_EnterReturnsTrue
					                          | ImGuiInputTextFlags_AutoSelectAll
					                          | ImGuiInputTextFlags_NoHorizontalScroll
					                          | ImGuiInputTextFlags_CallbackAlways
					                          | ImGuiInputTextFlags_AlwaysOverwrite;
					ImGui::SetNextItemWidth(s.glyphWidth * 2);
					if (ImGui::InputText("##data", dataInputBuf.data(), dataInputBuf.size(), flags, UserData::Callback, &user_data)) {
						data_write = data_next = true;
					} else if (!dataEditingTakeFocus && !ImGui::IsItemActive()) {
						nextAddr.reset();
					}
					dataEditingTakeFocus = false;
					if (user_data.CursorPos >= 2) {
						data_write = data_next = true;
					}
					if (nextAddr) {
						data_write = data_next = false;
					}
					unsigned int data_input_value = 0;
					if (data_write && sscanf(dataInputBuf.data(), "%X", &data_input_value) == 1) {
						debuggable.write(addr, uint8_t(data_input_value));
					}
					ImGui::PopID();
				} else {
					// NB: The trailing space is not visible but ensure there's no gap that the mouse cannot click on.
					uint8_t b = debuggable.read(addr);

					if (b == 0 && greyOutZeroes) {
						ImGui::TextDisabled("00 ");
					} else {
						ImGui::StrCat(hex_string<2, HexCase::upper>(b), ' ');
					}
					if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
						dataEditingTakeFocus = true;
						nextAddr = addr;
					}
				}
			}

			if (showAscii) {
				// Draw ASCII values
				ImGui::SameLine(s.posAsciiStart);
				ImVec2 pos = ImGui::GetCursorScreenPos();
				addr = unsigned(line_i) * columns;
				ImGui::PushID(line_i);
				if (ImGui::InvisibleButton("ascii", ImVec2(s.posAsciiEnd - s.posAsciiStart, s.lineHeight))) {
					currentAddr = addr + unsigned((ImGui::GetIO().MousePos.x - pos.x) / s.glyphWidth);
					currentAddr = std::min(currentAddr, memSize - 1);
					dataEditingTakeFocus = true;
				}
				ImGui::PopID();
				for (int n = 0; n < columns && addr < memSize; ++n, ++addr) {
					if (addr == currentAddr) {
						draw_list->AddRectFilled(pos, ImVec2(pos.x + s.glyphWidth, pos.y + s.lineHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
						draw_list->AddRectFilled(pos, ImVec2(pos.x + s.glyphWidth, pos.y + s.lineHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
					}
					uint8_t c = debuggable.read(addr);
					char display_c = (c < 32 || c >= 128) ? '.' : char(c);
					draw_list->AddText(pos, (display_c == char(c)) ? color_text : color_disabled, &display_c, &display_c + 1);
					pos.x += s.glyphWidth;
				}
			}
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::EndChild();

	// Notify the main window of our ideal child content size (FIXME: we are missing an API to get the contents size from the child)
	ImGui::SetCursorPosX(s.windowWidth);

	if (data_next && currentAddr + 1 < memSize) {
		++currentAddr;
		dataEditingTakeFocus = true;
	} else if (nextAddr) {
		currentAddr = *nextAddr;
		dataEditingTakeFocus = true;
	}

	const bool lock_show_data_preview = showDataPreview;
	ImGui::Separator();
	drawOptionsLine(s, memSize);

	if (lock_show_data_preview) {
		ImGui::Separator();
		drawPreviewLine(s, debuggable, memSize);
	}
}

void DebuggableEditor::drawOptionsLine(const Sizes& s, unsigned memSize)
{
	const auto& style = ImGui::GetStyle();

	// Options menu
	if (ImGui::Button("Options")) {
		ImGui::OpenPopup("context");
	}
	if (ImGui::BeginPopup("context")) {
		ImGui::SetNextItemWidth(7.5f * s.glyphWidth + 2.0f * style.FramePadding.x);
		if (ImGui::InputInt("Columns", &columns, 1, 0)) {
			contentsWidthChanged = true;
			columns = std::clamp(columns, 1, 64);
		}
		ImGui::Checkbox("Show Data Preview", &showDataPreview);
		if (ImGui::Checkbox("Show Ascii", &showAscii)) {
			contentsWidthChanged = true;
		}
		ImGui::Checkbox("Grey out zeroes", &greyOutZeroes);

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("Address: ");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(float(s.addrDigitsCount + 1) * s.glyphWidth + 2.0f * style.FramePadding.x);
	if (ImGui::InputText("##addr", addrInputBuf.data(), addrInputBuf.size(), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
		unsigned gotoAddr;
		if (sscanf(addrInputBuf.data(), "%X", &gotoAddr) == 1) {
			gotoAddr = std::min(gotoAddr, memSize - 1);

			ImGui::BeginChild("##scrolling");
			int row = gotoAddr / columns;
			ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + float(row) * ImGui::GetTextLineHeight());
			ImGui::EndChild();
			currentAddr = gotoAddr;
			dataEditingTakeFocus = true;
		}
	}
}

[[nodiscard]] static const char* DataTypeGetDesc(ImGuiDataType data_type)
{
	std::array<const char*, ImGuiDataType_COUNT - 2> desc = { "Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64" };
	assert(data_type >= 0 && data_type < (ImGuiDataType_COUNT - 2));
	return desc[data_type];
}

template<typename T>
[[nodiscard]] static T read(std::span<const uint8_t> buf)
{
	assert(buf.size() >= sizeof(T));
	T t = 0;
	memcpy(&t, buf.data(), sizeof(T));
	return t;
}

static void formatDec(std::span<const uint8_t> buf, ImGuiDataType data_type)
{
	switch (data_type) {
	case ImGuiDataType_S8:
		ImGui::StrCat(read<int8_t>(buf));
		break;
	case ImGuiDataType_U8:
		ImGui::StrCat(read<uint8_t>(buf));
		break;
	case ImGuiDataType_S16:
		ImGui::StrCat(read<int16_t>(buf));
		break;
	case ImGuiDataType_U16:
		ImGui::StrCat(read<uint16_t>(buf));
		break;
	case ImGuiDataType_S32:
		ImGui::StrCat(read<int32_t>(buf));
		break;
	case ImGuiDataType_U32:
		ImGui::StrCat(read<uint32_t>(buf));
		break;
	case ImGuiDataType_S64:
		ImGui::StrCat(read<int64_t>(buf));
		break;
	case ImGuiDataType_U64:
		ImGui::StrCat(read<uint64_t>(buf));
		break;
	default:
		UNREACHABLE;
	}
}

static void formatHex(std::span<const uint8_t> buf, ImGuiDataType data_type)
{
	switch (data_type) {
	case ImGuiDataType_S8:
	case ImGuiDataType_U8:
		ImGui::StrCat(hex_string<2>(read<uint8_t>(buf)));
		break;
	case ImGuiDataType_S16:
	case ImGuiDataType_U16:
		ImGui::StrCat(hex_string<4>(read<uint16_t>(buf)));
		break;
	case ImGuiDataType_S32:
	case ImGuiDataType_U32:
		ImGui::StrCat(hex_string<8>(read<uint32_t>(buf)));
		break;
	case ImGuiDataType_S64:
	case ImGuiDataType_U64:
		ImGui::StrCat(hex_string<16>(read<uint64_t>(buf)));
		break;
	default:
		UNREACHABLE;
	}
}

static void formatBin(std::span<const uint8_t> buf)
{
	for (int i = int(buf.size()) - 1; i >= 0; --i) {
		ImGui::StrCat(bin_string<8>(buf[i]));
		if (i != 0) ImGui::SameLine();
	}
}

void DebuggableEditor::drawPreviewLine(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	const auto& style = ImGui::GetStyle();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Preview as:"sv);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((s.glyphWidth * 10.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
	if (ImGui::BeginCombo("##combo_type", DataTypeGetDesc(previewDataType), ImGuiComboFlags_HeightLargest)) {
		for (int n = 0; n < (ImGuiDataType_COUNT - 2); ++n) {
			if (ImGui::Selectable(DataTypeGetDesc((ImGuiDataType)n), previewDataType == n)) {
				previewDataType = ImGuiDataType(n);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth((s.glyphWidth * 6.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
	ImGui::Combo("##combo_endianess", &previewEndianess, "LE\0BE\0\0");

	std::array<uint8_t, 8> dataBuf = {};
	auto elem_size = DataTypeGetSize(previewDataType);
	for (auto i : xrange(elem_size)) {
		auto addr = currentAddr + i;
		dataBuf[i] = (addr < memSize) ? debuggable.read(addr) : 0;
	}

	static constexpr bool native_is_little = std::endian::native == std::endian::little;
	bool preview_is_little = previewEndianess == 0;
	if (native_is_little != preview_is_little) {
		std::reverse(dataBuf.begin(), dataBuf.begin() + elem_size);
	}

	ImGui::TextUnformatted("Dec "sv);
	ImGui::SameLine();
	formatDec(dataBuf, previewDataType);

	ImGui::TextUnformatted("Hex "sv);
	ImGui::SameLine();
	formatHex(dataBuf, previewDataType);

	ImGui::TextUnformatted("Bin "sv);
	ImGui::SameLine();
	formatBin(subspan(dataBuf, 0, elem_size));
}

} // namespace openmsx
