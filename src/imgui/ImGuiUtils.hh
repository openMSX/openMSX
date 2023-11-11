#ifndef IMGUI_UTILS_HH
#define IMGUI_UTILS_HH

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"

#include "Reactor.hh"

#include "ranges.hh"
#include "strCat.hh"
#include "StringOp.hh"
#include "StringReplacer.hh"

#include <imgui.h>

#include <algorithm>
#include <concepts>
#include <functional>
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

template<typename... Ts>
void StrCat(Ts&& ...ts)
{
	auto s = tmpStrCat(std::forward<Ts>(ts)...);
	TextUnformatted(std::string_view(s));
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

void drawURL(std::string_view text, zstring_view url);

struct GetSettingDescription {
	std::string operator()(const Setting& setting) const;
};

bool Checkbox(const HotKey& hotkey, BooleanSetting& setting);
bool Checkbox(const HotKey& hotkey, const char* label, BooleanSetting& setting, std::function<std::string(const Setting&)> getTooltip = GetSettingDescription{});
bool SliderInt(IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderFloat(FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool InputText(Setting& setting);
bool InputText(const char* label, Setting& setting);
void ComboBox(Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, Setting& setting, std::function<std::string(const std::string&)> displayValue, EnumToolTips toolTips = {});
void ComboBox(VideoSourceSetting& setting);
void ComboBox(const char* label, VideoSourceSetting& setting);

const char* getComboString(int item, const char* itemsSeparatedByZeros);

std::string formatTime(double time);
float calculateFade(float current, float target, float period);

template<int HexDigits>
void comboHexSequence(const char* label, int* value, int mult) {
	*value &= ~(mult - 1);
	auto preview = tmpStrCat("0x", hex_string<HexDigits>(*value));
	im::Combo(label, preview.c_str(), [&]{
		for (int addr = 0; addr < 0x1ffff; addr += mult) {
			auto str = tmpStrCat("0x", hex_string<HexDigits>(addr));
			if (ImGui::Selectable(str.c_str())) {
				*value = addr;
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
void applyComboFilter(std::string_view key, const std::string& value, const std::vector<T>& items, std::vector<size_t>& indices)
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
		return !ranges::all_of(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(filterString, ' '),
			[&](auto part) { return StringOp::containsCaseInsensitive(name, part); });
	});
}

template<typename T>
void applyDisplayNameFilter(std::string_view filterString, const std::vector<T>& items, std::vector<size_t>& indices)
{
	filterIndices(filterString, [&](size_t idx) { return items[idx].displayName; }, indices);
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

// Parse machine or extension config files:
//  store config-name, display-name and meta-data
template<typename InfoType>
std::vector<InfoType> parseAllConfigFiles(ImGuiManager& manager, std::string_view type, std::initializer_list<std::string_view> topics)
{
	static constexpr auto replacer = StringReplacer::create(
		"name",         "Name",
		"manufacturer", "Manufacturer",
		"code",         "Product code",
		"region",       "Region",
		"release_year", "Release year",
		"description",  "Description",
		"type",         "Type");

	std::vector<InfoType> result;

	const auto& configs = Reactor::getHwConfigs(type);
	result.reserve(configs.size());
	for (const auto& config : configs) {
		auto& info = result.emplace_back();
		info.configName = config;

		// get machine meta-data
		auto& configInfo = info.configInfo;
		if (auto r = manager.execute(makeTclList("openmsx_info", type, config))) {
			auto first = r->begin();
			auto last = r->end();
			while (first != last) {
				auto desc = *first++;
				if (first == last) break; // shouldn't happen
				auto value = *first++;
				if (!value.empty()) {
					configInfo.emplace_back(std::string(replacer(desc)),
								std::string(value));
				}
			}
		}

		// Based on the above meta-data, try to construct a more
		// readable name Unfortunately this new name is no
		// longer guaranteed to be unique, we'll address this
		// below.
		auto& display = info.displayName;
		for (const auto topic : topics) {
			if (const auto* value = getOptionalDictValue(configInfo, topic)) {
				if (!value->empty()) {
					if (!display.empty()) strAppend(display, ' ');
					strAppend(display, *value);
				}
			}
		}
		if (display.empty()) display = config;
	}

	ranges::sort(result, StringOp::caseless{}, &InfoType::displayName);

	// make 'displayName' unique again
	auto sameDisplayName = [](InfoType& x, InfoType& y) {
		StringOp::casecmp cmp;
		return cmp(x.displayName, y.displayName);
	};
	chunk_by(result, sameDisplayName, [](auto first, auto last) {
		if (std::distance(first, last) == 1) return; // no duplicate name
		for (auto it = first; it != last; ++it) {
			strAppend(it->displayName, " (", it->configName, ')');
		}
		ranges::sort(first, last, StringOp::caseless{}, &InfoType::displayName);
	});

	return result;
}

std::string getShortCutForCommand(const HotKey& hotkey, std::string_view command);

} // namespace openmsx

#endif
