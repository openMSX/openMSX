#ifndef IMGUI_UTILS_HH
#define IMGUI_UTILS_HH

#include "ImGuiCpp.hh"

#include "strCat.hh"

#include <imgui.h>

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

bool Checkbox(BooleanSetting& setting);
bool Checkbox(const char* label, BooleanSetting& setting);
bool SliderInt(IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderFloat(FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool InputText(Setting& setting);
bool InputText(const char* label, Setting& setting);
void ComboBox(Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
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

} // namespace openmsx

#endif
