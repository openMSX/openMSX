#ifndef IMGUI_UTILS_HH
#define IMGUI_UTILS_HH

#include <span>
#include <string>
#include <string_view>

using ImGuiSliderFlags = int;

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

void simpleToolTip(const char* desc);
inline void simpleToolTip(const std::string& desc) { simpleToolTip(desc.c_str()); }
inline void simpleToolTip(std::string_view desc) { simpleToolTip(std::string(desc)); }

void HelpMarker(const char* desc);

bool Checkbox(const char* label, BooleanSetting& setting);
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags = 0);
bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool InputText(const char* label, Setting& setting);
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips = {}); // must be an EnumSetting
void ComboBox(const char* label, VideoSourceSetting& setting);

std::string formatTime(double time);
float calculateFade(float current, float target, float period);

} // namespace openmsx

#endif
