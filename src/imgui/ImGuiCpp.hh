#ifndef IMGUI_CPP_HH
#define IMGUI_CPP_HH

#include <imgui.h>

#include <concepts>

// C++ wrapper around the Dear ImGui C-API
//
// Several Dear ImGui functions always need to be called in pairs. Some examples:
// * ImGui::Begin() always needs to be paired with ImGui::End().
// * Similar for ImGui::TreeNodeEx() and ImGui::TreePop().
//
// These wrapper ensure that you cannot do a wrong pairing.
//
// In case of the former 'End()' must always be called (whether 'Begin()'
// returned 'true' or 'false'). But for the latter 'TreePop()' may only be
// called when 'TreeNodeEx()' returned 'true'. This difference only exists for
// historic reason (and thus for backwards compatibility). The Dear ImGui
// documentation states that in some future version this difference will likely
// be removed. These wrappers already hide this difference today.
//
// Example usage:
// * This uses the original Dear ImGui C-API:
//     if (ImGui::Begin("my-window")) {
//         ImGui::BeginDisabled(b);
//         if (ImGui::TreeNodeEx("bla", flag)) {
//             ... more stuff
//             ImGui::TreePop();
//         }
//         if (ImGui::TreeNodeEx("foobar")) {
//             ... more stuff
//             ImGui::TreePop();
//         }
//         ImGui::EndDisabled();
//     }
//     ImGui::End();
//
// * This is the exact same example, but with our C++ wrapper functions:
//     im::Window("my-window", [&]{
//         im::Disabled(b, [&]{
//             im::TreeNode("bla", flag, [&]{
//                 ... more stuff
//             });
//             im::TreeNode("foobar", [&]{
//                 ... more stuff
//             });
//         });
//     });
//
// This wrapper is inspired by ideas from:
//   https://github.com/ocornut/imgui/issues/2096
//   https://github.com/ocornut/imgui/pull/2197
//   https://github.com/mnesarco/imgui_sugar
//   https://github.com/kfsone/imguiwrap

namespace im {

// im::Window(): wrapper around ImGui::Begin() / ImGui::End()
inline void Window(const char* name, bool* p_open, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	if (ImGui::Begin(name, p_open, flags)) {
		next();
	}
	ImGui::End();
}
inline void Window(const char* name, bool* p_open, std::invocable<> auto next)
{
	Window(name, p_open, 0, next);
}
inline void Window(const char* name, std::invocable<> auto next)
{
	Window(name, nullptr, 0, next);
}

// im::Child(): wrapper around ImGui::BeginChild() / ImGui::EndChild()
inline void Child(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	if (ImGui::BeginChild(str_id, size, border, flags)) {
		next();
	}
	ImGui::EndChild();
}
inline void Child(const char* str_id, const ImVec2& size, bool border, std::invocable<> auto next)
{
	Child(str_id, size, border, 0, next);
}
inline void Child(const char* str_id, const ImVec2& size, std::invocable<> auto next)
{
	Child(str_id, size, false, 0, next);
}
inline void Child(const char* str_id, std::invocable<> auto next)
{
	Child(str_id, {}, false, 0, next);
}

// im::Font(): wrapper around ImGui::PushFont() / ImGui::PopFont()
inline void Font(ImFont* font, std::invocable<> auto next)
{
	ImGui::PushFont(font);
	next();
	ImGui::PopFont();
}

// im::StyleColor(): wrapper around ImGui::PushStyleColor() / ImGui::PopStyleColor()
// Add more overloads when needed
inline void StyleColor(ImGuiCol idx, ImVec4 col, std::invocable<> auto next)
{
	ImGui::PushStyleColor(idx, col);
	next();
	ImGui::PopStyleColor(1);
}
inline void StyleColor(ImGuiCol idx1, ImVec4 col1,
                       ImGuiCol idx2, ImVec4 col2,
                       std::invocable<> auto next)
{
	ImGui::PushStyleColor(idx1, col1);
	ImGui::PushStyleColor(idx2, col2);
	next();
	ImGui::PopStyleColor(2);
}
inline void StyleColor(ImGuiCol idx1, ImVec4 col1,
                       ImGuiCol idx2, ImVec4 col2,
                       ImGuiCol idx3, ImVec4 col3,
                       std::invocable<> auto next)
{
	ImGui::PushStyleColor(idx1, col1);
	ImGui::PushStyleColor(idx2, col2);
	ImGui::PushStyleColor(idx3, col3);
	next();
	ImGui::PopStyleColor(3);
}
inline void StyleColor(ImGuiCol idx1, ImVec4 col1,
                       ImGuiCol idx2, ImVec4 col2,
                       ImGuiCol idx3, ImVec4 col3,
                       ImGuiCol idx4, ImVec4 col4,
                       std::invocable<> auto next)
{
	ImGui::PushStyleColor(idx1, col1);
	ImGui::PushStyleColor(idx2, col2);
	ImGui::PushStyleColor(idx3, col3);
	ImGui::PushStyleColor(idx4, col4);
	next();
	ImGui::PopStyleColor(4);
}
inline void StyleColor(ImGuiCol idx, ImU32 col, std::invocable<> auto next)
{
	StyleColor(idx, ImGui::ColorConvertU32ToFloat4(col), next);
}

// im::StyleVar(): wrapper around ImGui::PushStyleVar() / ImGui::PopStyleVar()
// Add more overloads when needed
inline void StyleVar(ImGuiStyleVar idx, float val, std::invocable<> auto next)
{
	ImGui::PushStyleVar(idx, val);
	next();
	ImGui::PopStyleVar(1);
}
inline void StyleVar(ImGuiStyleVar idx, const ImVec2& val, std::invocable<> auto next)
{
	ImGui::PushStyleVar(idx, val);
	next();
	ImGui::PopStyleVar(1);
}

// im::ItemWidth(): wrapper around ImGui::PushItemWidth() / ImGui::PopItemWidth()
inline void ItemWidth(float item_width, std::invocable<> auto next)
{
	ImGui::PushItemWidth(item_width);
	next();
	ImGui::PopItemWidth();
}

// im::TextWrapPos(): wrapper around ImGui::PushTextWrapPos() / ImGui::PopTextWrapPos()
inline void TextWrapPos(float wrap_local_pos_x, std::invocable<> auto next)
{
	ImGui::PushTextWrapPos(wrap_local_pos_x);
	next();
	ImGui::PopTextWrapPos();
}
inline void TextWrapPos(std::invocable<> auto next)
{
	TextWrapPos(0.0f, next);
}

// im::Indent(): wrapper around ImGui::Indent() / ImGui::Unindent()
inline void Indent(float indent_w, std::invocable<> auto next)
{
	ImGui::Indent(indent_w);
	next();
	ImGui::Unindent(indent_w);
}
inline void Indent(std::invocable<> auto next)
{
	Indent(0.0f, next);
}

// im::Group(): wrapper around ImGui::BeginGroup() / ImGui::EndGroup()
inline void Group(std::invocable<> auto next)
{
	ImGui::BeginGroup();
	next();
	ImGui::EndGroup();
}

// im::ID(): wrapper around ImGui::PushID() / ImGui::PopID()
inline void ID(const char* str_id, std::invocable<> auto next)
{
	ImGui::PushID(str_id);
	next();
	ImGui::PopID();
}
inline void ID(const char* str_id_begin, const char* str_id_end, std::invocable<> auto next)
{
	ImGui::PushID(str_id_begin, str_id_end);
	next();
	ImGui::PopID();
}
inline void ID(const void* ptr_id, std::invocable<> auto next)
{
	ImGui::PushID(ptr_id);
	next();
	ImGui::PopID();
}
inline void ID(int int_id, std::invocable<> auto next)
{
	ImGui::PushID(int_id);
	next();
	ImGui::PopID();
}
inline void ID(const std::string& str, std::invocable<> auto next)
{
	auto begin = str.data();
	auto end = begin + str.size();
	ID(begin, end, next);
}
inline void ID(std::string_view str, std::invocable<> auto next)
{
	auto begin = str.data();
	auto end = begin + str.size();
	ID(begin, end, next);
}

// im::Combo(): wrapper around ImGui::BeginCombo() / ImGui::EndCombo()
inline void Combo(const char* label, const char* preview_value, ImGuiComboFlags flags, std::invocable<> auto next)
{
	if (ImGui::BeginCombo(label, preview_value, flags)) {
		next();
		ImGui::EndCombo();
	}
}
inline void Combo(const char* label, const char* preview_value, std::invocable<> auto next)
{
	Combo(label, preview_value, 0, next);
}

// im::TreeNode(): wrapper around ImGui::TreeNodeEx() / ImGui::TreePop()
inline void TreeNode(const char* label, ImGuiTreeNodeFlags flags, std::invocable<> auto next)
{
	if (ImGui::TreeNodeEx(label, flags)) {
		next();
		ImGui::TreePop();
	}
}
inline void TreeNode(const char* label, std::invocable<> auto next)
{
	TreeNode(label, 0, next);
}
inline void TreeNode(const char* label, bool* p_open, std::invocable<> auto next)
{
	assert(p_open);
	ImGui::SetNextItemOpen(*p_open);
	int flags = 0;
	*p_open = ImGui::TreeNodeEx(label, flags);
	if (*p_open) {
		next();
		ImGui::TreePop();
	}
}

// im::ListBox(): wrapper around ImGui::BeginListBox() / ImGui::EndListBox()
inline void ListBox(const char* label, const ImVec2& size, std::invocable<> auto next)
{
	if (ImGui::BeginListBox(label, size)) {
		next();
		ImGui::EndListBox();
	}
}
inline void ListBox(const char* label, std::invocable<> auto next)
{
	ListBox(label, {}, next);
}

// im::MainMenuBar(): wrapper around ImGui::BeginMainMenuBar() / ImGui::EndMainMenuBar()
inline void MainMenuBar(std::invocable<> auto next)
{
	if (ImGui::BeginMainMenuBar()) {
		next();
		ImGui::EndMainMenuBar();
	}
}

// im::Menu(): wrapper around ImGui::BeginMenu() / ImGui::EndMenu()
inline void Menu(const char* label, bool enabled, std::invocable<> auto next)
{
	if (ImGui::BeginMenu(label, enabled)) {
		next();
		ImGui::EndMenu();
	}
}
inline void Menu(const char* label, std::invocable<> auto next)
{
	Menu(label, true, next);
}

// im::Tooltip(): wrapper around ImGui::BeginTooltip() / ImGui::EndTooltip()
inline void Tooltip(std::invocable<> auto next)
{
	if (ImGui::BeginTooltip()) {
		next();
		ImGui::EndTooltip();
	}
}

// im::Popup(): wrapper around ImGui::BeginPopup() / ImGui::EndPopup()
inline void Popup(const char* str_id, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	if (ImGui::BeginPopup(str_id, flags)) {
		next();
		ImGui::EndPopup();
	}
}
inline void Popup(const char* str_id, std::invocable<> auto next)
{
	Popup(str_id, 0, next);
}

// im::PopupContextItem(): wrapper around ImGui::BeginPopupContextItem() / ImGui::EndPopup()
inline void PopupContextItem(const char* str_id, ImGuiPopupFlags popup_flags, std::invocable<> auto next)
{
	if (ImGui::BeginPopupContextItem(str_id, popup_flags)) {
		next();
		ImGui::EndPopup();
	}
}
inline void PopupContextItem(const char* str_id, std::invocable<> auto next)
{
	PopupContextItem(str_id, 1, next);
}
inline void PopupContextItem(std::invocable<> auto next)
{
	PopupContextItem(nullptr, 1, next);
}

// im::PopupContextWindow(): wrapper around ImGui::BeginPopupContextWindow() / ImGui::EndPopup()
inline void PopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags, std::invocable<> auto next)
{
	if (ImGui::BeginPopupContextWindow(str_id, popup_flags)) {
		next();
		ImGui::EndPopup();
	}
}
inline void PopupContextWindow(const char* str_id, std::invocable<> auto next)
{
	PopupContextItem(str_id, 1, next);
}
inline void PopupContextWindow(std::invocable<> auto next)
{
	PopupContextItem(nullptr, 1, next);
}

// im::Table(): wrapper around ImGui::BeginTable() / ImGui::EndTable()
inline void Table(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width, std::invocable<> auto next)
{
	if (ImGui::BeginTable(str_id, column, flags, outer_size, inner_width)) {
		next();
		ImGui::EndTable();
	}
}
inline void Table(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, std::invocable<> auto next)
{
	Table(str_id, column, flags, outer_size, 0.0f, next);
}
inline void Table(const char* str_id, int column, ImGuiTableFlags flags, std::invocable<> auto next)
{
	Table(str_id, column, flags, {}, 0.0f, next);
}
inline void Table(const char* str_id, int column, std::invocable<> auto next)
{
	Table(str_id, column, 0, {}, 0.0f, next);
}

// im::TabBar(): wrapper around ImGui::BeginTabBar() / ImGui::EndTabBar()
inline void TabBar(const char* str_id, ImGuiTabBarFlags flags, std::invocable<> auto next)
{
	if (ImGui::BeginTabBar(str_id, flags)) {
		next();
		ImGui::EndTabBar();
	}
}
inline void TabBar(const char* str_id, std::invocable<> auto next)
{
	TabBar(str_id, 0, next);
}

// im::TabItem(): wrapper around ImGui::BeginTabItem() / ImGui::EndTabItem()
inline void TabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags, std::invocable<> auto next)
{
	if (ImGui::BeginTabItem(label, p_open, flags)) {
		next();
		ImGui::EndTabItem();
	}
}
inline void TabItem(const char* label, bool* p_open, std::invocable<> auto next)
{
	TabItem(label, p_open, 0, next);
}
inline void TabItem(const char* label, std::invocable<> auto next)
{
	TabItem(label, nullptr, 0, next);
}

// im::Disabled(): wrapper around ImGui::BeginDisabled() / ImGui::EndDisabled()
inline void Disabled(bool b, std::invocable<> auto next)
{
	ImGui::BeginDisabled(b);
	next();
	ImGui::EndDisabled();
}

} // namespace im

#endif
