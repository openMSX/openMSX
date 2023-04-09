#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VideoSourceSetting.hh"

#include "ranges.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace openmsx {

void simpleToolTip(const char* desc)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}


static void settingStuff(Setting& setting)
{
	simpleToolTip(setting.getDescription());
	if (ImGui::BeginPopupContextItem()) {
		auto defaultValue = setting.getDefaultValue();
		auto defaultString = defaultValue.getString();
		ImGui::Text("Default value: %s", defaultString.c_str());
		if (defaultString.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("<empty>");
		}
		if (ImGui::Button("Restore default")) {
			setting.setValue(defaultValue);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

bool Checkbox(const char* label, BooleanSetting& setting)
{
	bool value = setting.getBoolean();
	bool changed = ImGui::Checkbox(label, &value);
	if (changed) setting.setBoolean(value);
	settingStuff(setting);
	return changed;
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

bool InputText(const char* label, Setting& setting)
{
	auto value = std::string(setting.getValue().getString());
	bool changed = ImGui::InputText(label, &value);
	if (changed) setting.setValue(TclObject(value));
	settingStuff(setting);
	return changed;
}

void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips)
{
	auto* enumSetting = dynamic_cast<EnumSettingBase*>(&setting);
	assert(enumSetting);
	auto current = setting.getValue().getString();
	if (ImGui::BeginCombo(label, current.c_str())) {
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
		ImGui::EndCombo();
	}
	settingStuff(setting);
}

void ComboBox(const char* label, VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	auto current = setting.getValue().getString();
	if (ImGui::BeginCombo(label, current.c_str())) {
		for (const auto& value : setting.getPossibleValues()) {
			bool selected = value == current;
			if (ImGui::Selectable(std::string(value).c_str(), selected)) {
				setting.setValue(TclObject(value));
			}
		}
		ImGui::EndCombo();
	}
	settingStuff(setting);
}

std::string formatTime(double time)
{
	assert(time >= 0.0);
	int hours = int(time / 3600.0);
	time -= double(hours * 3600);
	int minutes = int(time / 60.0);
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
