#ifndef IMGUI_UTILS_HH
#define IMGUI_UTILS_HH

#include "ImGuiCpp.hh"

#include "Reactor.hh"

#include "function_ref.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "StringOp.hh"
#include "circular_buffer.hh"

#include <imgui.h>
#include <imgui_internal.h> // ImTextCharToUtf8

#include <algorithm>
#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ImGui {

inline void TextUnformatted(const std::string& str)
{
	const char* begin = str.data();
	const char* end = begin + str.size();
	ImGui::TextUnformatted(begin, end);
}
inline void TextUnformatted(std::string_view str)
{
	const char* begin = str.data();
	const char* end = begin + str.size();
	ImGui::TextUnformatted(begin, end);
}

inline auto CalcTextSize(std::string_view str)
{
	return ImGui::CalcTextSize(str.data(), str.data() + str.size());
}

template<typename... Ts>
void StrCat(Ts&& ...ts)
{
	auto s = tmpStrCat(std::forward<Ts>(ts)...);
	TextUnformatted(std::string_view(s));
}

inline void RightAlignText(std::string_view text, std::string_view maxWidthText)
{
	auto maxWidth = ImGui::CalcTextSize(maxWidthText).x;
	auto actualWidth = ImGui::CalcTextSize(text).x;
	if (auto spacing = maxWidth - actualWidth; spacing > 0.0f) {
		auto pos = ImGui::GetCursorPosX();
		ImGui::SetCursorPosX(pos + spacing);
	}
	ImGui::TextUnformatted(text);
}

} // namespace ImGui

namespace openmsx {

class BooleanSetting;
class FloatSetting;
class HotKey;
class IntegerSetting;
class Setting;
class VideoSourceSetting;

struct EnumToolTip {
	std::string_view value;
	std::string_view tip;
};
using EnumToolTips = std::span<const EnumToolTip>;

inline void simpleToolTip(std::string_view desc)
{
	if (desc.empty()) return;
	im::ItemTooltip([&]{
		im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&]{
			ImGui::TextUnformatted(desc);
		});
	});
}

void simpleToolTip(std::invocable<> auto descFunc)
{
	if (!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) return;
	auto desc = std::invoke(descFunc);
	if (desc.empty()) return;
	im::Tooltip([&]{
		im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&]{
			ImGui::TextUnformatted(desc);
		});
	});
}

void HelpMarker(std::string_view desc);

inline bool ButtonWithCustomRendering(
	const char* label, gl::vec2 size, bool pressed,
	std::invocable<gl::vec2 /*center*/, ImDrawList*> auto render)
{
	bool result = false;
	im::StyleColor(pressed, ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive), [&]{
		gl::vec2 topLeft = ImGui::GetCursorScreenPos();
		gl::vec2 center = topLeft + size * 0.5f;
		result = ImGui::Button(label, size);
		render(center, ImGui::GetWindowDrawList());
	});
	return result;
}

inline bool ButtonWithCustomRendering(
	const char* label, gl::vec2 size,
	std::invocable<gl::vec2 /*center*/, ImDrawList*> auto render)
{
	return ButtonWithCustomRendering(label, size, false, render);
}

inline bool ButtonWithCenteredGlyph(ImWchar glyph, gl::vec2 maxGlyphSize)
{
	std::array<char, 2 + 5> label = {'#', '#'};
	ImTextCharToUtf8(&label[2], glyph);

	const auto& style = ImGui::GetStyle();
	auto buttonSize = maxGlyphSize + 2.0f* gl::vec2(style.FramePadding);

	return ButtonWithCustomRendering(label.data(), buttonSize, [&](gl::vec2 center, ImDrawList* drawList) {
		const auto* font = ImGui::GetFont();
		auto texId = font->ContainerAtlas->TexID;
		const auto* g = font->FindGlyph(glyph);
		auto halfSize = gl::vec2{g->X1 - g->X0, g->Y1 - g->Y0} * 0.5f;
		drawList->AddImage(texId, center - halfSize, center + halfSize, {g->U0, g->V0}, {g->U1, g->V1});
	});
};

inline void centerNextWindowOverCurrent()
{
	static constexpr gl::vec2 center{0.5f, 0.5f};
	gl::vec2 windowPos = ImGui::GetWindowPos();
	gl::vec2 windowSize = ImGui::GetWindowSize();
	auto windowCenter = windowPos + center * windowSize;
	ImGui::SetNextWindowPos(windowCenter, ImGuiCond_Appearing, center);
}

struct GetSettingDescription {
	std::string operator()(const Setting& setting) const;
};

bool Checkbox(const HotKey& hotkey, BooleanSetting& setting);
bool Checkbox(const HotKey& hotkey, const char* label, BooleanSetting& setting, function_ref<std::string(const Setting&)> getTooltip = GetSettingDescription{});
bool SliderInt(IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderFloat(FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool InputText(Setting& setting);
bool InputText(const char* label, Setting& setting);
void ComboBox(Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, Setting& setting, function_ref<std::string(const std::string&)> displayValue, EnumToolTips toolTips = {});
void ComboBox(VideoSourceSetting& setting);
void ComboBox(const char* label, VideoSourceSetting& setting);

const char* getComboString(int item, const char* itemsSeparatedByZeros);

std::string formatTime(std::optional<double> time);
float calculateFade(float current, float target, float period);

template<int HexDigits>
void comboHexSequence(const char* label, int* value, int mult, int max, int offset) {
	assert(offset < mult);
	*value &= ~(mult - 1);
	// only apply offset in display, not in the actual value
	auto preview = tmpStrCat("0x", hex_string<HexDigits>(*value | offset));
	im::Combo(label, preview.c_str(), [&]{
		for (int addr = 0; addr < max; addr += mult) {
			if (auto str = tmpStrCat("0x", hex_string<HexDigits>(addr | offset));
			    ImGui::Selectable(str.c_str(), *value == addr)) {
				*value = addr;
			}
			if (*value == addr) {
				ImGui::SetItemDefaultFocus();
			}
		}
	});
};

template<typename Range, typename Projection>
void sortUpDown_T(Range& range, const ImGuiTableSortSpecs* sortSpecs, Projection proj) {
	if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending) {
		ranges::stable_sort(range, std::greater<>{}, proj);
	} else {
		ranges::stable_sort(range, std::less<>{}, proj);
	}
};
template<typename Range, typename Projection>
void sortUpDown_String(Range& range, const ImGuiTableSortSpecs* sortSpecs, Projection proj) {
	if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending) {
		ranges::stable_sort(range, StringOp::inv_caseless{}, proj);
	} else {
		ranges::stable_sort(range, StringOp::caseless{}, proj);
	}
};


[[nodiscard]] inline const std::string* getOptionalDictValue(
	const std::vector<std::pair<std::string, std::string>>& info,
	std::string_view key)
{
	auto it = ranges::find_if(info, [&](const auto& p) { return p.first == key; });
	if (it == info.end()) return {};
	return &it->second;
}

template<typename T> // 'MachineInfo' or 'ExtensionInfo', both have a 'configInfo' member
[[nodiscard]] std::vector<std::string> getAllValuesFor(std::string_view key, const std::vector<T>& items)
{
	std::vector<std::string> result;
	for (const auto& item : items) {
		if (const auto* value = getOptionalDictValue(item.configInfo, key)) {
			if (!contains(result, *value)) { // O(N^2), but that's fine
				result.emplace_back(*value);
			}
		}
	}
	ranges::sort(result, StringOp::caseless{});
	return result;
}

template<typename T>
void displayFilterCombo(std::string& selection, zstring_view key, const std::vector<T>& items)
{
	im::Combo(key.c_str(), selection.empty() ? "--all--" : selection.c_str(), [&]{
		if (ImGui::Selectable("--all--")) {
			selection.clear();
		}
		for (const auto& type : getAllValuesFor(key, items)) {
			if (ImGui::Selectable(type.c_str())) {
				selection = type;
			}
		}
	});
}

template<typename T>
void applyComboFilter(std::string_view key, std::string_view value, const std::vector<T>& items, std::vector<size_t>& indices)
{
	if (value.empty()) return;
	std::erase_if(indices, [&](auto idx) {
		const auto& info = items[idx].configInfo;
		const auto* val = getOptionalDictValue(info, key);
		if (!val) return true; // remove items that don't have the key
		return *val != value;
	});
}

template<std::invocable<size_t> GetName>
void filterIndices(std::string_view filterString, GetName getName, std::vector<size_t>& indices)
{
	if (filterString.empty()) return;
	std::erase_if(indices, [&](auto idx) {
		const auto& name = getName(idx);
		return !ranges::all_of(StringOp::split_view<StringOp::EmptyParts::REMOVE>(filterString, ' '),
			[&](auto part) { return StringOp::containsCaseInsensitive(name, part); });
	});
}

template<typename T>
void applyDisplayNameFilter(std::string_view filterString, const std::vector<T>& items, std::vector<size_t>& indices)
{
	filterIndices(filterString, [&](size_t idx) { return items[idx].displayName; }, indices);
}

template<typename T>
void addRecentItem(circular_buffer<T>& recentItems, const T& item)
{
	if (auto it = ranges::find(recentItems, item); it != recentItems.end()) {
		// was already present, move to front
		std::rotate(recentItems.begin(), it, it + 1);
	} else {
		// new entry, add it, but possibly remove oldest entry
		if (recentItems.full()) recentItems.pop_back();
		recentItems.push_front(item);
	}
}

// Similar to c++23 chunk_by(). Main difference is internal vs external iteration.
template<typename Range, typename BinaryPred, typename Action>
static void chunk_by(Range&& range, BinaryPred pred, Action action)
{
	auto it = std::begin(range);
	auto last = std::end(range);
	while (it != last) {
		auto start = it;
		auto prev = it++;
		while (it != last && pred(*prev, *it)) {
			prev = it++;
		}
		action(start, it);
	}
}

std::string getShortCutForCommand(const HotKey& hotkey, std::string_view command);

std::string getKeyChordName(ImGuiKeyChord keyChord);
std::optional<ImGuiKeyChord> parseKeyChord(std::string_view name);

// Read from VRAM-table, including mirroring behavior
//  shared between ImGuiCharacter, ImGuiSpriteViewer
class VramTable {
public:
	VramTable(std::span<const uint8_t> vram_, bool planar_ = false)
		: vram(vram_), planar(planar_) {}

	void setRegister(unsigned value, unsigned extraLsbBits) {
		registerMask = (value << extraLsbBits) | ~(~0u << extraLsbBits);
	}
	void setIndexSize(unsigned bits) {
		indexMask = ~0u << bits;
	}

	[[nodiscard]] auto getAddress(unsigned index) const {
		return registerMask & (indexMask | index);
	}
	[[nodiscard]] uint8_t operator[](unsigned index) const {
		auto addr = getAddress(index);
		if (planar) {
			addr = ((addr << 16) | (addr >> 1)) & 0x1'FFFF;
		}
		return vram[addr];
	}
private:
	std::span<const uint8_t> vram;
	unsigned registerMask = 0;
	unsigned indexMask = 0;
	bool planar = false;
};

enum class imColor : unsigned {
	TRANSPARENT,
	BLACK,
	WHITE,
	GRAY,
	YELLOW,
	RED_BG,    // red    background (transparent)
	YELLOW_BG, // yellow background (transparent)

	TEXT,
	TEXT_DISABLED,

	ERROR,
	WARNING,

	COMMENT, // syntax highlighting in the console
	VARIABLE,
	LITERAL,
	PROC,
	OPERATOR,

	KEY_ACTIVE,     // virtual keyboard
	KEY_NOT_ACTIVE,

	NUM_COLORS
};
inline std::array<ImU32, size_t(imColor::NUM_COLORS)> imColors;

void setColors(int style);

inline ImU32 getColor(imColor col) {
	assert(col < imColor::NUM_COLORS);
	return imColors[size_t(col)];
}

} // namespace openmsx

#endif
