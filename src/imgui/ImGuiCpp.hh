#ifndef IMGUI_CPP_HH
#define IMGUI_CPP_HH

#include "narrow.hh"
#include "xrange.hh"

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
inline bool Window(const char* name, bool* p_open, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::Begin(name, p_open, flags);
	if (open) {
		next();
	}
	ImGui::End();
	return open;
}
inline bool Window(const char* name, bool* p_open, std::invocable<> auto next)
{
	return Window(name, p_open, 0, next);
}
inline bool Window(const char* name, std::invocable<> auto next)
{
	return Window(name, nullptr, 0, next);
}

struct WindowStatus {
	bool open = false; // [level] true <=> window is open
	bool do_raise = false; // [edge]  set to true when you want to raise this window
	                       //         gets automatically reset when done
	void raise() { do_raise = open = true; }
};
inline bool Window(const char* name, WindowStatus& status, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::Begin(name, &status.open, flags);
	if (open) {
		if (status.do_raise) {
			status.do_raise = false;
			if (!ImGui::IsWindowAppearing()) { // otherwise crash, viewport not yet initialized???
				// depending on the backend (e.g. Wayland) this could be nullptr
				if (auto* swf = ImGui::GetPlatformIO().Platform_SetWindowFocus) {
					swf(ImGui::GetWindowViewport());
				}
				// When window is inside the main viewport, this function is
				// needed to raise focus even if Platform_SetWindowFocus exists
				ImGui::SetWindowFocus();
			}
		}
		next();
	}
	ImGui::End();
	return open;
}
inline bool Window(const char* name, WindowStatus& status, std::invocable<> auto next)
{
	return Window(name, status, 0, next);
}

// im::Child(): wrapper around ImGui::BeginChild() / ImGui::EndChild()
inline bool Child(const char* str_id, const ImVec2& size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginChild(str_id, size, child_flags, window_flags);
	if (open) {
		next();
	}
	ImGui::EndChild();
	return open;
}
inline bool Child(const char* str_id, const ImVec2& size, ImGuiChildFlags child_flags, std::invocable<> auto next)
{
	return Child(str_id, size, child_flags, 0, next);
}
inline bool Child(const char* str_id, const ImVec2& size, std::invocable<> auto next)
{
	return Child(str_id, size, 0, 0, next);
}
inline bool Child(const char* str_id, std::invocable<> auto next)
{
	return Child(str_id, {}, 0, 0, next);
}

// im::Font(): wrapper around ImGui::PushFont() / ImGui::PopFont()
inline void Font(ImFont* font, std::invocable<> auto next)
{
	ImGui::PushFont(font);
	next();
	ImGui::PopFont();
}
// Same functionality as im::Font(), but different usage (sometimes one sometimes the other is easier).
struct ScopedFont {
	explicit ScopedFont(ImFont* font) { ImGui::PushFont(font); }
	~ScopedFont() { ImGui::PopFont(); }

	ScopedFont(const ScopedFont&) = delete;
	ScopedFont(ScopedFont&&) = delete;
	ScopedFont& operator=(const ScopedFont&) = delete;
	ScopedFont& operator=(ScopedFont&&) = delete;
};

// im::StyleColor(): wrapper around ImGui::PushStyleColor() / ImGui::PopStyleColor()
// Can be called like:
//   im::StyleColor(
//      bool active,                  // _optional_ boolean parameter
//      ImGuiCol idx1, ImVec4 col1,   // followed by an arbitrary number of [idx, col] pairs
//      ImGuiCol idx2, ImU32 col2,    //   where col can either be 'ImVec4' or 'ImU32'
//      ...
//      std::invocable<> auto next);  // and finally a lambda that will be executed with these color changes
template<int N>
inline void StyleColor_impl(bool active, std::invocable<> auto next)
{
	next();
	if (active) ImGui::PopStyleColor(N);
}
template<int N, typename... Args>
inline void StyleColor_impl(bool active, ImGuiCol idx, ImVec4 col, Args&& ...args)
{
	if (active) ImGui::PushStyleColor(idx, col);
	StyleColor_impl<N>(active, std::forward<Args>(args)...);
}
template<int N, typename... Args>
inline void StyleColor_impl(bool active, ImGuiCol idx, ImU32 col, Args&& ...args)
{
	if (active) ImGui::PushStyleColor(idx, ImGui::ColorConvertU32ToFloat4(col));
	StyleColor_impl<N>(active, std::forward<Args>(args)...);
}
template<typename... Args>
inline void StyleColor(bool active, Args&& ...args)
{
	static constexpr auto N = sizeof...(args);
	static_assert(N >= 3);
	static_assert((N & 1) == 1);
	StyleColor_impl<(N - 1) / 2>(active, std::forward<Args>(args)...);
}
template<typename... Args>
inline void StyleColor(Args&& ...args)
{
	StyleColor(true, std::forward<Args>(args)...);
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
	const auto* begin = str.data();
	const auto* end = begin + str.size();
	ID(begin, end, next);
}
inline void ID(std::string_view str, std::invocable<> auto next)
{
	const auto* begin = str.data();
	const auto* end = begin + str.size();
	ID(begin, end, next);
}

inline void ID_for_range(std::integral auto count, std::invocable<int> auto next)
{
	for (auto i : xrange(narrow<int>(count))) {
		ID(i, [&]{ next(i); });
	}
}

// im::Combo(): wrapper around ImGui::BeginCombo() / ImGui::EndCombo()
inline bool Combo(const char* label, const char* preview_value, ImGuiComboFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginCombo(label, preview_value, flags);
	if (open) {
		next();
		ImGui::EndCombo();
	}
	return open;
}
inline bool Combo(const char* label, const char* preview_value, std::invocable<> auto next)
{
	return Combo(label, preview_value, 0, next);
}

// im::TreeNode(): wrapper around ImGui::TreeNodeEx() / ImGui::TreePop()
inline bool TreeNode(const char* label, ImGuiTreeNodeFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::TreeNodeEx(label, flags);
	if (open) {
		next();
		ImGui::TreePop();
	}
	return open;
}
inline bool TreeNode(const char* label, std::invocable<> auto next)
{
	return TreeNode(label, 0, next);
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
inline bool ListBox(const char* label, const ImVec2& size, std::invocable<> auto next)
{
	bool open = ImGui::BeginListBox(label, size);
	if (open) {
		next();
		ImGui::EndListBox();
	}
	return open;
}
inline bool ListBox(const char* label, std::invocable<> auto next)
{
	return ListBox(label, {}, next);
}

// im::MainMenuBar(): wrapper around ImGui::BeginMenuBar() / ImGui::EndMenuBar()
inline bool MenuBar(std::invocable<> auto next)
{
	bool open = ImGui::BeginMenuBar();
	if (open) {
		next();
		ImGui::EndMenuBar();
	}
	return open;
}

// im::MainMenuBar(): wrapper around ImGui::BeginMainMenuBar() / ImGui::EndMainMenuBar()
inline bool MainMenuBar(std::invocable<> auto next)
{
	bool open = ImGui::BeginMainMenuBar();
	if (open) {
		next();
		ImGui::EndMainMenuBar();
	}
	return open;
}

// im::Menu(): wrapper around ImGui::BeginMenu() / ImGui::EndMenu()
inline bool Menu(const char* label, bool enabled, std::invocable<> auto next)
{
	bool open = ImGui::BeginMenu(label, enabled);
	if (open) {
		next();
		ImGui::EndMenu();
	}
	return open;
}
inline bool Menu(const char* label, std::invocable<> auto next)
{
	return Menu(label, true, next);
}

// im::Tooltip(): wrapper around ImGui::BeginTooltip() / ImGui::EndTooltip()
inline bool Tooltip(std::invocable<> auto next)
{
	bool open = ImGui::BeginTooltip();
	if (open) {
		next();
		ImGui::EndTooltip();
	}
	return open;
}
// im::Tooltip(): wrapper around ImGui::BeginItemTooltip() / ImGui::EndTooltip()
inline bool ItemTooltip(std::invocable<> auto next)
{
	bool open = ImGui::BeginItemTooltip();
	if (open) {
		next();
		ImGui::EndTooltip();
	}
	return open;
}

// im::Popup(): wrapper around ImGui::BeginPopup() / ImGui::EndPopup()
inline bool Popup(const char* str_id, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginPopup(str_id, flags);
	if (open) {
		next();
		ImGui::EndPopup();
	}
	return open;
}
inline bool Popup(const char* str_id, std::invocable<> auto next)
{
	return Popup(str_id, 0, next);
}

// im::PopupModal(): wrapper around ImGui::BeginPopupModal() / ImGui::EndPopup()
inline bool PopupModal(const char* name, bool* p_open, ImGuiWindowFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginPopupModal(name, p_open, flags);
	if (open) {
		next();
		ImGui::EndPopup();
	}
	return open;
}
inline bool PopupModal(const char* name, bool* p_open, std::invocable<> auto next)
{
	return PopupModal(name, p_open, 0, next);
}
inline bool PopupModal(const char* name, std::invocable<> auto next)
{
	return PopupModal(name, nullptr, 0, next);
}

// im::PopupContextItem(): wrapper around ImGui::BeginPopupContextItem() / ImGui::EndPopup()
inline bool PopupContextItem(const char* str_id, ImGuiPopupFlags popup_flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginPopupContextItem(str_id, popup_flags);
	if (open) {
		next();
		ImGui::EndPopup();
	}
	return open;
}
inline bool PopupContextItem(const char* str_id, std::invocable<> auto next)
{
	return PopupContextItem(str_id, ImGuiPopupFlags_MouseButtonRight, next);
}
inline bool PopupContextItem(std::invocable<> auto next)
{
	return PopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight, next);
}

// im::PopupContextWindow(): wrapper around ImGui::BeginPopupContextWindow() / ImGui::EndPopup()
inline bool PopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginPopupContextWindow(str_id, popup_flags);
	if (open) {
		next();
		ImGui::EndPopup();
	}
	return open;
}
inline bool PopupContextWindow(const char* str_id, std::invocable<> auto next)
{
	return PopupContextWindow(str_id, 1, next);
}
inline bool PopupContextWindow(std::invocable<> auto next)
{
	return PopupContextWindow(nullptr, 1, next);
}

// im::Table(): wrapper around ImGui::BeginTable() / ImGui::EndTable()
inline bool Table(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width, std::invocable<> auto next)
{
	bool open = ImGui::BeginTable(str_id, column, flags, outer_size, inner_width);
	if (open) {
		next();
		ImGui::EndTable();
	}
	return open;
}
inline bool Table(const char* str_id, int column, ImGuiTableFlags flags, const ImVec2& outer_size, std::invocable<> auto next)
{
	return Table(str_id, column, flags, outer_size, 0.0f, next);
}
inline bool Table(const char* str_id, int column, ImGuiTableFlags flags, std::invocable<> auto next)
{
	return Table(str_id, column, flags, {}, 0.0f, next);
}
inline bool Table(const char* str_id, int column, std::invocable<> auto next)
{
	return Table(str_id, column, 0, {}, 0.0f, next);
}

// im::TabBar(): wrapper around ImGui::BeginTabBar() / ImGui::EndTabBar()
inline bool TabBar(const char* str_id, ImGuiTabBarFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginTabBar(str_id, flags);
	if (open) {
		next();
		ImGui::EndTabBar();
	}
	return open;
}
inline bool TabBar(const char* str_id, std::invocable<> auto next)
{
	return TabBar(str_id, 0, next);
}

// im::TabItem(): wrapper around ImGui::BeginTabItem() / ImGui::EndTabItem()
inline bool TabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags, std::invocable<> auto next)
{
	bool open = ImGui::BeginTabItem(label, p_open, flags);
	if (open) {
		next();
		ImGui::EndTabItem();
	}
	return open;
}
inline bool TabItem(const char* label, bool* p_open, std::invocable<> auto next)
{
	return TabItem(label, p_open, 0, next);
}
inline bool TabItem(const char* label, std::invocable<> auto next)
{
	return TabItem(label, nullptr, 0, next);
}

// im::Disabled(): wrapper around ImGui::BeginDisabled() / ImGui::EndDisabled()
inline void Disabled(bool b, std::invocable<> auto next)
{
	ImGui::BeginDisabled(b);
	next();
	ImGui::EndDisabled();
}

// im::DisabledIndent(): combination of Disabled() and Indent()
inline void DisabledIndent(bool b, std::invocable<> auto next)
{
	ImGui::BeginDisabled(b);
	ImGui::Indent(); // TODO for now not configurable
	next();
	ImGui::Unindent();
	ImGui::EndDisabled();
}

// im::VisuallyDisabled(): similar to Disabled(), but only visually disable, still allow to interact normally
inline void VisuallyDisabled(bool b, std::invocable<> auto next)
{
	if (b) {
		const auto& style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
	}
	next();
	if (b) {
		ImGui::PopStyleVar(1);
	}
}

// im::ListClipper: wrapper around ImGuiListClipper
// hides the typical nested loop
inline void ListClipper(size_t count, int forceIndex, float lineHeight, std::invocable<int> auto next)
{
	ImGuiListClipper clipper; // only draw the actually visible lines
	clipper.Begin(narrow<int>(count), lineHeight);
	if (forceIndex > 0) clipper.IncludeItemByIndex(narrow<int>(forceIndex));
	while (clipper.Step()) {
		for (int i : xrange(clipper.DisplayStart, clipper.DisplayEnd)) {
			next(i);
		}
	}
}
inline void ListClipper(size_t count, int forceIndex, std::invocable<int> auto next)
{
	ListClipper(count, forceIndex, -1.0f, next);
}
inline void ListClipper(size_t count, std::invocable<int> auto next)
{
	ListClipper(count, -1, next);
}

// im::ListClipperID: combination of im::ListClipper() and im::ID()
inline void ListClipperID(size_t count, std::invocable<int> auto next)
{
	ImGuiListClipper clipper; // only draw the actually visible lines
	clipper.Begin(narrow<int>(count));
	while (clipper.Step()) {
		for (int i : xrange(clipper.DisplayStart, clipper.DisplayEnd)) {
			ImGui::PushID(i);
			next(i);
			ImGui::PopID();
		}
	}
}

} // namespace im

#endif
