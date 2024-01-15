// Based on:
//     Mini memory editor for Dear ImGui (to embed in your game/tools)
//
//     Get latest version at http://www.github.com/ocornut/imgui_club
//     (See there for documentation and the (old) Changelog).
//
//     Licence: MIT

// Todo/Bugs:
// - This is generally old/crappy code, it should work but isn't very good.. to be rewritten some day.
// - PageUp/PageDown are supported because we use _NoNav. This is a good test scenario for working out idioms of how to mix natural nav and our own...
// - Arrows are being sent to the InputText() about to disappear which for LeftArrow makes the text cursor appear at position 1 for one frame.
// - Using InputText() is awkward and maybe overkill here, consider implementing something custom.

#ifndef DEBUGGABLE_EDITOR_HH
#define DEBUGGABLE_EDITOR_HH

#include "ImGuiUtils.hh"

#include "unreachable.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <span>

using namespace std::literals; // TODO not in .hh

struct MemoryEditor
{
	static constexpr int MidColsCount = 8; // extra spacing between every mid-cols.
	static constexpr auto HighlightColor = IM_COL32(255, 255, 255, 50); // background color of highlighted bytes.

	// Settings
	bool     Open = true;                   // set to false when DrawWindow() was closed. ignore if not using DrawWindow().
	int      Cols = 16;                     // number of columns to display.
	bool     OptShowDataPreview = false;    // display a footer previewing the decimal/binary/hex/float representation of the currently selected bytes.
	bool     OptShowAscii = true;           // display ASCII representation on the right side.
	bool     OptGreyOutZeroes = true;       // display null/zero bytes using the TextDisabled color.
	int      OptAddrDigitsCount = 0;        // number of addr digits to display (default calculated based on maximum displayed addr).
	uint8_t  (*ReadFn)(const uint8_t* data, unsigned off) = nullptr;       // optional handler to read bytes.
	void     (*WriteFn)(uint8_t* data, unsigned off, uint8_t d) = nullptr; // optional handler to write bytes.

	// [Internal State]
	bool          ContentsWidthChanged = false;
	unsigned        currentAddr = 0;
	bool          DataEditingTakeFocus = true;
	std::array<char, 32> DataInputBuf = {};
	std::array<char, 32> AddrInputBuf = {};
	int           PreviewEndianess = 0;
	ImGuiDataType PreviewDataType = ImGuiDataType_U8;

	struct Sizes {
		int AddrDigitsCount = 0;
		float LineHeight = 0.0f;
		float GlyphWidth = 0.0f;
		float HexCellWidth = 0.0f;
		float SpacingBetweenMidCols = 0.0f;
		float PosHexStart = 0.0f;
		float PosHexEnd = 0.0f;
		float PosAsciiStart = 0.0f;
		float PosAsciiEnd = 0.0f;
		float WindowWidth = 0.0f;
	};

	Sizes CalcSizes(unsigned mem_size)
	{
		Sizes s;
		const auto& style = ImGui::GetStyle();
		s.AddrDigitsCount = OptAddrDigitsCount;
		if (s.AddrDigitsCount == 0) {
			for (unsigned n = mem_size - 1; n > 0; n >>= 4) {
				s.AddrDigitsCount++;
			}
		}
		s.LineHeight = ImGui::GetTextLineHeight();
		s.GlyphWidth = ImGui::CalcTextSize("F").x + 1;            // We assume the font is mono-space
		s.HexCellWidth = truncf(s.GlyphWidth * 2.5f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
		s.SpacingBetweenMidCols = truncf(s.HexCellWidth * 0.25f); // Every OptMidColsCount columns we add a bit of extra spacing
		s.PosHexStart = float(s.AddrDigitsCount + 2) * s.GlyphWidth;
		s.PosHexEnd = s.PosHexStart + (s.HexCellWidth * float(Cols));
		s.PosAsciiStart = s.PosAsciiEnd = s.PosHexEnd;
		if (OptShowAscii) {
			s.PosAsciiStart = s.PosHexEnd + s.GlyphWidth * 1;
			int numMacroColumns = (Cols + MidColsCount - 1) / MidColsCount;
			s.PosAsciiStart += float(numMacroColumns) * s.SpacingBetweenMidCols;
			s.PosAsciiEnd = s.PosAsciiStart + float(Cols) * s.GlyphWidth;
		}
		s.WindowWidth = s.PosAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.GlyphWidth;
		return s;
	}

	// Standalone Memory Editor window
	void DrawWindow(const char* title, void* mem_data, unsigned mem_size)
	{
		auto s = CalcSizes(mem_size);
		ImGui::SetNextWindowSize(ImVec2(s.WindowWidth, s.WindowWidth * 0.60f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(s.WindowWidth, FLT_MAX));

		Open = true;
		if (ImGui::Begin(title, &Open, ImGuiWindowFlags_NoScrollbar)) {
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				ImGui::OpenPopup("context");
			}
			DrawContents(mem_data, mem_size);
			if (ContentsWidthChanged) {
				s = CalcSizes(mem_size);
				ImGui::SetWindowSize(ImVec2(s.WindowWidth, ImGui::GetWindowSize().y));
			}
		}
		ImGui::End();
	}

	void DrawContents(void* mem_data_void, unsigned mem_size)
	{
		Cols = std::max(Cols, 1);

		auto* mem_data = static_cast<uint8_t*>(mem_data_void);
		auto s = CalcSizes(mem_size);
		const auto& style = ImGui::GetStyle();

		// We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
		// This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.
		const auto height_separator = style.ItemSpacing.y;
		float footer_height = height_separator + ImGui::GetFrameHeightWithSpacing();
		if (OptShowDataPreview) {
			footer_height += height_separator + ImGui::GetFrameHeightWithSpacing() + 3 * ImGui::GetTextLineHeightWithSpacing();
		}
		ImGui::BeginChild("##scrolling", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
		auto* draw_list = ImGui::GetWindowDrawList();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		// We are not really using the clipper API correctly here, because we rely on visible_start_addr/visible_end_addr for our scrolling function.
		const int line_total_count = int((mem_size + Cols - 1) / Cols);
		ImGuiListClipper clipper;
		clipper.Begin(line_total_count, s.LineHeight);

		bool data_next = false;

		if (currentAddr >= mem_size) {
			currentAddr = 0;
		}

		std::optional<unsigned> nextAddr;
		// Move cursor but only apply on next frame so scrolling with be synchronized (because currently we can't change the scrolling while the window is being rendered)
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) &&
			ptrdiff_t(currentAddr) >= ptrdiff_t(Cols)) {
			nextAddr = currentAddr - Cols;
		} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) &&
				ptrdiff_t(currentAddr) < ptrdiff_t(mem_size - Cols)) {
			nextAddr = currentAddr + Cols;
		} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) &&
				ptrdiff_t(currentAddr) > ptrdiff_t(0)) {
			nextAddr = currentAddr - 1;
		} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) &&
				ptrdiff_t(currentAddr) < ptrdiff_t(mem_size - 1)) {
			nextAddr = currentAddr + 1;
		}

		// Draw vertical separator
		ImVec2 window_pos = ImGui::GetWindowPos();
		if (OptShowAscii) {
			draw_list->AddLine(ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y),
			                   ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y + 9999),
			                   ImGui::GetColorU32(ImGuiCol_Border));
		}

		const auto color_text = ImGui::GetColorU32(ImGuiCol_Text);
		const auto color_disabled = OptGreyOutZeroes ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : color_text;

		while (clipper.Step()) {
			for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; ++line_i) {
				auto addr = unsigned(line_i) * Cols;
				ImGui::Text("%0*X: ", s.AddrDigitsCount, addr);

				// Draw Hexadecimal
				for (int n = 0; n < Cols && addr < mem_size; ++n, ++addr) {
					float byte_pos_x = s.PosHexStart + s.HexCellWidth * float(n);
					int macroColumn = n / MidColsCount;
					byte_pos_x += float(macroColumn) * s.SpacingBetweenMidCols;
					ImGui::SameLine(byte_pos_x);

					// Draw highlight
					auto preview_data_type_size = DataTypeGetSize(PreviewDataType);
					auto highLight = [&](unsigned a) {
						return (currentAddr <= a) && (a < (currentAddr + preview_data_type_size));
					};
					if (highLight(addr)) {
						ImVec2 pos = ImGui::GetCursorScreenPos();
						float highlight_width = s.GlyphWidth * 2;
						if (highLight(addr + 1)) {
							highlight_width = s.HexCellWidth;
							if (n > 0 && (n + 1) < Cols && ((n + 1) % MidColsCount) == 0) {
								highlight_width += s.SpacingBetweenMidCols;
							}
						}
						draw_list->AddRectFilled(pos, ImVec2(pos.x + highlight_width, pos.y + s.LineHeight), HighlightColor);
					}

					if (currentAddr == addr) {
						// Display text input on current byte
						bool data_write = false;
						ImGui::PushID(reinterpret_cast<void*>(addr));
						if (DataEditingTakeFocus) {
							ImGui::SetKeyboardFocusHere(0);
							sprintf(AddrInputBuf.data(), "%0*X", s.AddrDigitsCount, addr);
							sprintf(DataInputBuf.data(), "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
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
						sprintf(user_data.CurrentBufOverwrite.data(), "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
						ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal
						                          | ImGuiInputTextFlags_EnterReturnsTrue
						                          | ImGuiInputTextFlags_AutoSelectAll
						                          | ImGuiInputTextFlags_NoHorizontalScroll
						                          | ImGuiInputTextFlags_CallbackAlways
						                          | ImGuiInputTextFlags_AlwaysOverwrite;
						ImGui::SetNextItemWidth(s.GlyphWidth * 2);
						if (ImGui::InputText("##data", DataInputBuf.data(), DataInputBuf.size(), flags, UserData::Callback, &user_data)) {
							data_write = data_next = true;
						} else if (!DataEditingTakeFocus && !ImGui::IsItemActive()) {
							nextAddr.reset();
						}
						DataEditingTakeFocus = false;
						if (user_data.CursorPos >= 2) {
							data_write = data_next = true;
						}
						if (nextAddr) {
							data_write = data_next = false;
						}
						unsigned int data_input_value = 0;
						if (data_write && sscanf(DataInputBuf.data(), "%X", &data_input_value) == 1) {
							if (WriteFn) {
								WriteFn(mem_data, addr, uint8_t(data_input_value));
							} else {
								mem_data[addr] = uint8_t(data_input_value);
							}
						}
						ImGui::PopID();
					} else {
						// NB: The trailing space is not visible but ensure there's no gap that the mouse cannot click on.
						uint8_t b = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];

						if (b == 0 && OptGreyOutZeroes) {
							ImGui::TextDisabled("00 ");
						} else {
							ImGui::Text("%02X ", b);
						}
						if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
							DataEditingTakeFocus = true;
							nextAddr = addr;
						}
					}
				}

				if (OptShowAscii) {
					// Draw ASCII values
					ImGui::SameLine(s.PosAsciiStart);
					ImVec2 pos = ImGui::GetCursorScreenPos();
					addr = unsigned(line_i) * Cols;
					ImGui::PushID(line_i);
					if (ImGui::InvisibleButton("ascii", ImVec2(s.PosAsciiEnd - s.PosAsciiStart, s.LineHeight))) {
						currentAddr = addr + unsigned((ImGui::GetIO().MousePos.x - pos.x) / s.GlyphWidth);
						currentAddr = std::min(currentAddr, mem_size - 1);
						DataEditingTakeFocus = true;
					}
					ImGui::PopID();
					for (int n = 0; n < Cols && addr < mem_size; ++n, ++addr) {
						if (addr == currentAddr) {
							draw_list->AddRectFilled(pos, ImVec2(pos.x + s.GlyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
							draw_list->AddRectFilled(pos, ImVec2(pos.x + s.GlyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
						}
						unsigned char c = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];
						char display_c = (c < 32 || c >= 128) ? '.' : char(c);
						draw_list->AddText(pos, (display_c == char(c)) ? color_text : color_disabled, &display_c, &display_c + 1);
						pos.x += s.GlyphWidth;
					}
				}
			}
		}
		ImGui::PopStyleVar(2);
		ImGui::EndChild();

		// Notify the main window of our ideal child content size (FIXME: we are missing an API to get the contents size from the child)
		ImGui::SetCursorPosX(s.WindowWidth);

		if (data_next && currentAddr + 1 < mem_size) {
			++currentAddr;
			DataEditingTakeFocus = true;
		} else if (nextAddr) {
			currentAddr = *nextAddr;
			DataEditingTakeFocus = true;
		}

		const bool lock_show_data_preview = OptShowDataPreview;
		ImGui::Separator();
		DrawOptionsLine(s, mem_size);

		if (lock_show_data_preview) {
			ImGui::Separator();
			DrawPreviewLine(s, mem_data, mem_size);
		}
	}

	void DrawOptionsLine(const Sizes& s, unsigned mem_size)
	{
		const auto& style = ImGui::GetStyle();
		const char* format_range = "Range %0*X..%0*X";

		// Options menu
		if (ImGui::Button("Options")) {
			ImGui::OpenPopup("context");
		}
		if (ImGui::BeginPopup("context")) {
			ImGui::SetNextItemWidth(7.0f * s.GlyphWidth + 2.0f * style.FramePadding.x);
			if (ImGui::DragInt("##cols", &Cols, 0.2f, 4, 32, "%d cols")) {
				ContentsWidthChanged = true;
				Cols = std::max(Cols, 1);
			}
			ImGui::Checkbox("Show Data Preview", &OptShowDataPreview);
			if (ImGui::Checkbox("Show Ascii", &OptShowAscii)) {
				ContentsWidthChanged = true;
			}
			ImGui::Checkbox("Grey out zeroes", &OptGreyOutZeroes);

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Text(format_range, s.AddrDigitsCount, 0, s.AddrDigitsCount, mem_size - 1);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(float(s.AddrDigitsCount + 1) * s.GlyphWidth + 2.0f * style.FramePadding.x);
		if (ImGui::InputText("##addr", AddrInputBuf.data(), AddrInputBuf.size(), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
			unsigned gotoAddr;
			if (sscanf(AddrInputBuf.data(), "%X", &gotoAddr) == 1) {
				gotoAddr = std::min(gotoAddr, mem_size - 1);

				ImGui::BeginChild("##scrolling");
				int row = gotoAddr / Cols;
				ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + float(row) * ImGui::GetTextLineHeight());
				ImGui::EndChild();
				currentAddr = gotoAddr;
				DataEditingTakeFocus = true;
			}
		}
	}

	void DrawPreviewLine(const Sizes& s, void* mem_data_void, unsigned mem_size)
	{
		auto* mem_data = static_cast<uint8_t*>(mem_data_void);
		const auto& style = ImGui::GetStyle();
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Preview as:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth((s.GlyphWidth * 10.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
		if (ImGui::BeginCombo("##combo_type", DataTypeGetDesc(PreviewDataType), ImGuiComboFlags_HeightLargest)) {
			for (int n = 0; n < (ImGuiDataType_COUNT - 2); ++n) {
				if (ImGui::Selectable(DataTypeGetDesc((ImGuiDataType)n), PreviewDataType == n)) {
					PreviewDataType = ImGuiDataType(n);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth((s.GlyphWidth * 6.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
		ImGui::Combo("##combo_endianess", &PreviewEndianess, "LE\0BE\0\0");

		std::array<uint8_t, 8> dataBuf = {};
		auto elem_size = DataTypeGetSize(PreviewDataType);
		auto size = currentAddr + elem_size > mem_size ? mem_size - currentAddr : elem_size;
		if (ReadFn) {
			for (int i = 0, n = (int)size; i < n; ++i) {
				dataBuf[i] = ReadFn(mem_data, currentAddr + i);
			}
		} else {
			memcpy(dataBuf.data(), mem_data + currentAddr, size);
		}

		static constexpr bool native_is_little = std::endian::native == std::endian::little;
		bool preview_is_little = PreviewEndianess == 0;
		if (native_is_little != preview_is_little) {
			std::reverse(dataBuf.begin(), dataBuf.begin() + elem_size);
		}

		ImGui::TextUnformatted("Dec "sv);
		ImGui::SameLine();
		formatDec(dataBuf, PreviewDataType);

		ImGui::TextUnformatted("Hex "sv);
		ImGui::SameLine();
		formatHex(dataBuf, PreviewDataType);

		ImGui::TextUnformatted("Bin "sv);
		ImGui::SameLine();
		formatBin(subspan(dataBuf, 0, elem_size));
	}

	[[nodiscard]] static const char* DataTypeGetDesc(ImGuiDataType data_type)
	{
		std::array<const char*, ImGuiDataType_COUNT - 2> desc = { "Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64" };
		assert(data_type >= 0 && data_type < (ImGuiDataType_COUNT - 2));
		return desc[data_type];
	}

	[[nodiscard]] static unsigned DataTypeGetSize(ImGuiDataType data_type)
	{
		std::array<unsigned, ImGuiDataType_COUNT - 2> sizes = { 1, 1, 2, 2, 4, 4, 8, 8 };
		assert(data_type >= 0 && data_type < (ImGuiDataType_COUNT - 2));
		return sizes[data_type];
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
};

#endif
