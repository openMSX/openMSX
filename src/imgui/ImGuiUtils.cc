#include "ImGuiUtils.hh"

#include "ImGuiCpp.hh"

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VideoSourceSetting.hh"

#include "ranges.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace openmsx {

void HelpMarker(std::string_view desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}


static void settingStuff(Setting& setting)
{
	simpleToolTip(setting.getDescription());
	im::PopupContextItem([&]{
		auto defaultValue = setting.getDefaultValue();
		auto defaultString = defaultValue.getString();
		ImGui::StrCat("Default value: ", defaultString);
		if (defaultString.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("<empty>");
		}
		if (ImGui::Button("Restore default")) {
			setting.setValue(defaultValue);
			ImGui::CloseCurrentPopup();
		}
	});
}

bool Checkbox(BooleanSetting& setting)
{
	std::string name(setting.getBaseName());
	return Checkbox(name.c_str(), setting);
}
bool Checkbox(const char* label, BooleanSetting& setting)
{
	bool value = setting.getBoolean();
	bool changed = ImGui::Checkbox(label, &value);
	if (changed) setting.setBoolean(value);
	settingStuff(setting);
	return changed;
}

bool SliderInt(IntegerSetting& setting, ImGuiSliderFlags flags)
{
	std::string name(setting.getBaseName());
	return SliderInt(name.c_str(), setting, flags);
}
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags)
{
	int value = setting.getInt();
	int min = setting.getMinValue();
	int max = setting.getMaxValue();
	bool changed = ImGui::SliderInt(label, &value, min, max, "%d", flags);
	if (changed) setting.setInt(value);
	settingStuff(setting);
	return changed;
}

bool SliderFloat(FloatSetting& setting, const char* format, ImGuiSliderFlags flags)
{
	std::string name(setting.getBaseName());
	return SliderFloat(name.c_str(), setting, format, flags);
}
bool SliderFloat(const char* label, FloatSetting& setting, const char* format, ImGuiSliderFlags flags)
{
	float value = setting.getFloat();
	float min = narrow_cast<float>(setting.getMinValue());
	float max = narrow_cast<float>(setting.getMaxValue());
	bool changed = ImGui::SliderFloat(label, &value, min, max, format, flags);
	if (changed) setting.setDouble(value); // TODO setFloat()
	settingStuff(setting);
	return changed;
}

bool InputText(Setting& setting)
{
	std::string name(setting.getBaseName());
	return InputText(name.c_str(), setting);
}
bool InputText(const char* label, Setting& setting)
{
	auto value = std::string(setting.getValue().getString());
	bool changed = ImGui::InputText(label, &value);
	if (changed) setting.setValue(TclObject(value));
	settingStuff(setting);
	return changed;
}

void ComboBox(Setting& setting, EnumToolTips toolTips)
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting, toolTips);
}
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips)
{
	auto* enumSetting = dynamic_cast<EnumSettingBase*>(&setting);
	assert(enumSetting);
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& entry : enumSetting->getMap()) {
			bool selected = entry.name == current;
			if (ImGui::Selectable(entry.name.c_str(), selected)) {
				setting.setValue(TclObject(entry.name));
			}
			if (auto it = ranges::find(toolTips, entry.name, &EnumToolTip::value);
			    it != toolTips.end()) {
				simpleToolTip(it->tip);
			}
		}
	});
	settingStuff(setting);
}

void ComboBox(VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting);
}
void ComboBox(const char* label, VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	std::string name(setting.getBaseName());
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& value : setting.getPossibleValues()) {
			bool selected = value == current;
			if (ImGui::Selectable(std::string(value).c_str(), selected)) {
				setting.setValue(TclObject(value));
			}
		}
	});
	settingStuff(setting);
}

const char* getComboString(int item, const char* itemsSeparatedByZeros)
{
	const char* p = itemsSeparatedByZeros;
	while (true) {
		assert(*p);
		if (item == 0) return p;
		--item;
		while (*p) ++p;
		++p;
	}
}

std::string formatTime(double time)
{
	assert(time >= 0.0);
	int hours = int(time * (1.0 / 3600.0));
	time -= double(hours * 3600);
	int minutes = int(time * (1.0 / 60.0));
	time -= double(minutes * 60);
	int seconds = int(time);
	time -= double(seconds);
	int hundreds = int(100.0 * time);

	std::string result = "00:00:00.00";
	auto insert = [&](size_t pos, unsigned value) {
		assert(value < 100);
		result[pos + 0] = char('0' + (value / 10));
		result[pos + 1] = char('0' + (value % 10));
	};
	insert(0, hours % 100);
	insert(3, minutes);
	insert(6, seconds);
	insert(9, hundreds);
	return result;
}

float calculateFade(float current, float target, float period)
{
	const auto& io = ImGui::GetIO();
	auto step = io.DeltaTime / period;
	if (target > current) {
		return std::min(target, current + step);
	} else {
		return std::max(target, current - step);
	}
}

} // namespace openmsx
