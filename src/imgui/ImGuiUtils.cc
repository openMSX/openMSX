#include "ImGuiUtils.hh"

#include "ImGuiCpp.hh"

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "HotKey.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VideoSourceSetting.hh"
#include "KeyMappings.hh"

#include "ranges.hh"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <SDL.h>

#include <variant>

namespace openmsx {

void HelpMarker(std::string_view desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}

std::string GetSettingDescription::operator()(const Setting& setting) const
{
	return std::string(setting.getDescription());
}

template<std::invocable<const Setting&> GetTooltip = GetSettingDescription>
static void settingStuff(Setting& setting, GetTooltip getTooltip = {})
{
	simpleToolTip([&] { return getTooltip(setting); });
	im::PopupContextItem([&]{
		auto defaultValue = setting.getDefaultValue();
		auto defaultString = defaultValue.getString();
		ImGui::StrCat("Default value: ", defaultString);
		if (defaultString.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("<empty>");
		}
		if (ImGui::Button("Restore default")) {
			try {
				setting.setValue(defaultValue);
			} catch (MSXException&) {
				// ignore
			}
			ImGui::CloseCurrentPopup();
		}
	});
}

bool Checkbox(const HotKey& hotKey, BooleanSetting& setting)
{
	std::string name(setting.getBaseName());
	return Checkbox(hotKey, name.c_str(), setting);
}
bool Checkbox(const HotKey& hotKey, const char* label, BooleanSetting& setting, function_ref<std::string(const Setting&)> getTooltip)
{
	bool value = setting.getBoolean();
	bool changed = ImGui::Checkbox(label, &value);
	try {
		if (changed) setting.setBoolean(value);
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting, getTooltip);

	ImGui::SameLine();
	auto shortCut = getShortCutForCommand(hotKey, strCat("toggle ", setting.getBaseName()));
	auto spacing = std::max(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(shortCut).x);
	ImGui::SameLine(0.0f, spacing);
	ImGui::TextDisabled("%s", shortCut.c_str());

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
	try {
		if (changed) setting.setInt(value);
	} catch (MSXException&) {
		// ignore
	}
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
	auto value = setting.getFloat();
	auto min = narrow_cast<float>(setting.getMinValue());
	auto max = narrow_cast<float>(setting.getMaxValue());
	bool changed = ImGui::SliderFloat(label, &value, min, max, format, flags);
	try {
		if (changed) setting.setFloat(value);
	} catch (MSXException&) {
		// ignore
	}
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
	bool changed = ImGui::InputText(label, &value, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit();
	try {
		if (changed) setting.setValue(TclObject(value));
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting);
	return changed;
}

void ComboBox(const char* label, Setting& setting, function_ref<std::string(const std::string&)> displayValue, EnumToolTips toolTips)
{
	const auto* enumSetting = dynamic_cast<const EnumSettingBase*>(&setting);
	assert(enumSetting);
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& entry : enumSetting->getMap()) {
			bool selected = entry.name == current;
			if (const auto& display = displayValue(entry.name);
			    ImGui::Selectable(display.c_str(), selected)) {
				try {
					setting.setValue(TclObject(entry.name));
				} catch (MSXException&) {
					// ignore
				}
			}
			if (auto it = ranges::find(toolTips, entry.name, &EnumToolTip::value);
			    it != toolTips.end()) {
				simpleToolTip(it->tip);
			}
		}
	});
	settingStuff(setting);
}
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips)
{
	ComboBox(label, setting, std::identity{}, toolTips);
}
void ComboBox(Setting& setting, EnumToolTips toolTips)
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting, toolTips);
}

void ComboBox(VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting);
}
void ComboBox(const char* label, VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& value : setting.getPossibleValues()) {
			bool selected = value == current;
			if (ImGui::Selectable(std::string(value).c_str(), selected)) {
				try {
					setting.setValue(TclObject(value));
				} catch (MSXException&) {
					// ignore
				}
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

std::string formatTime(std::optional<double> time)
{
	if (!time) return "--:--:--.--";
	auto remainingTime = *time;
	assert(remainingTime >= 0.0);
	auto hours = int(remainingTime * (1.0 / 3600.0));
	remainingTime -= double(hours * 3600);
	auto minutes = int(remainingTime * (1.0 / 60.0));
	remainingTime -= double(minutes * 60);
	auto seconds = int(remainingTime);
	remainingTime -= double(seconds);
	auto hundreds = int(100.0 * remainingTime);

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

std::string getShortCutForCommand(const HotKey& hotkey, std::string_view command)
{
	for (const auto& info : hotkey.getGlobalBindings()) {
		if (info.command != command) continue;
		if (const auto* keyDown = std::get_if<KeyDownEvent>(&info.event)) {
			std::string result;
			auto modifiers = keyDown->getModifiers();
			if (modifiers & KMOD_CTRL)  strAppend(result, "CTRL+");
			if (modifiers & KMOD_SHIFT) strAppend(result, "SHIFT+");
			if (modifiers & KMOD_ALT)   strAppend(result, "ALT+");
			if (modifiers & KMOD_GUI)   strAppend(result, "GUI+");
			strAppend(result, SDL_GetKeyName(keyDown->getKeyCode()));
			return result;
		}
	}
	return "";
}

[[nodiscard]] static std::string_view superName()
{
	return ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+" : "Super+";
}

std::string getKeyChordName(ImGuiKeyChord keyChord)
{
	int keyCode = ImGuiKey2SDL(ImGuiKey(keyChord & ~ImGuiMod_Mask_));
	const auto* name = SDL_GetKeyName(keyCode);
	if (!name || (*name == '\0')) return "None";
	return strCat(
		(keyChord & ImGuiMod_Ctrl  ? "Ctrl+"  : ""),
		(keyChord & ImGuiMod_Shift ? "Shift+" : ""),
		(keyChord & ImGuiMod_Alt   ? "Alt+"   : ""),
		(keyChord & ImGuiMod_Super ? superName() : ""),
		name);
}

std::optional<ImGuiKeyChord> parseKeyChord(std::string_view name)
{
	if (name == "None") return ImGuiKey_None;

	// Similar to "StringOp::splitOnLast(name, '+')", but includes the last '+'
	auto [modifiers, key] = [&]() -> std::pair<std::string_view, std::string_view> {
		if (auto pos = name.find_last_of('+'); pos == std::string_view::npos) {
			return {std::string_view{}, name};
		} else {
			return {name.substr(0, pos + 1), name.substr(pos + 1)};
		}
	}();

	SDL_Keycode keyCode = SDL_GetKeyFromName(std::string(key).c_str());
	if (keyCode == SDLK_UNKNOWN) return {};

	auto contains = [](std::string_view haystack, std::string_view needle) {
		// TODO in the future use c++23 std::string_view::contains()
		return haystack.find(needle) != std::string_view::npos;
	};
	ImGuiKeyChord keyMods =
		(contains(modifiers, "Ctrl+" ) ? ImGuiMod_Ctrl  : 0) |
		(contains(modifiers, "Shift+") ? ImGuiMod_Shift : 0) |
		(contains(modifiers, "Alt+"  ) ? ImGuiMod_Alt   : 0) |
		(contains(modifiers, superName()) ? ImGuiMod_Super : 0);

	return SDLKey2ImGui(keyCode) | keyMods;
}

void setColors(int style)
{
	// style: 0->dark, 1->light, 2->classic
	bool light = style == 1;
	using enum imColor;

	//                                   AA'BB'GG'RR
	imColors[size_t(TRANSPARENT   )] = 0x00'00'00'00;
	imColors[size_t(BLACK         )] = 0xff'00'00'00;
	imColors[size_t(WHITE         )] = 0xff'ff'ff'ff;
	imColors[size_t(GRAY          )] = 0xff'80'80'80;
	imColors[size_t(YELLOW        )] = 0xff'00'ff'ff;
	imColors[size_t(RED_BG        )] = 0x40'00'00'ff;
	imColors[size_t(YELLOW_BG     )] = 0x80'00'ff'ff;

	imColors[size_t(TEXT          )] = ImGui::GetColorU32(ImGuiCol_Text);
	imColors[size_t(TEXT_DISABLED )] = ImGui::GetColorU32(ImGuiCol_TextDisabled);

	imColors[size_t(ERROR         )] = 0xff'00'00'ff;
	imColors[size_t(WARNING       )] = 0xff'33'b3'ff;

	imColors[size_t(COMMENT       )] = 0xff'5c'ff'5c;
	imColors[size_t(VARIABLE      )] = 0xff'ff'ff'00;
	imColors[size_t(LITERAL       )] = light ? 0xff'9c'5d'27 : 0xff'00'ff'ff;
	imColors[size_t(PROC          )] = 0xff'cd'00'cd;
	imColors[size_t(OPERATOR      )] = 0xff'cd'cd'00;

	imColors[size_t(KEY_ACTIVE    )] = 0xff'10'40'ff;
	imColors[size_t(KEY_NOT_ACTIVE)] = 0x80'00'00'00;
}

} // namespace openmsx
