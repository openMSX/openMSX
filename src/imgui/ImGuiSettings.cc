#include "ImGuiSettings.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "FilenameSetting.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "IntegerSetting.hh"
#include "KeyCodeSetting.hh"
#include "Mixer.hh"
#include "ProxySetting.hh"
#include "Reactor.hh"
#include "ReadOnlySetting.hh"
#include "SettingsManager.hh"
#include "StringSetting.hh"
#include "VideoSourceSetting.hh"
#include "checked_cast.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace openmsx {

void ImGuiSettings::showMenu(MSXMotherBoard* /*motherBoard*/)
{
	if (ImGui::BeginMenu("Settings")) {
		auto& reactor = manager.getReactor();
		if (ImGui::BeginMenu("Video")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Sound")) {
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			ImGui::BeginDisabled(muteSetting.getBoolean());
			SliderInt("master volume", mixer.getMasterVolume());
			ImGui::EndDisabled();
			Checkbox("mute", muteSetting);
			ImGui::Separator();
			ImGui::MenuItem("Show sound chip settings", nullptr, &manager.soundChip.showSoundChipSettings);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Misc")) {
			ImGui::MenuItem("Show OSD icons", nullptr, &manager.osdIcons.showIcons);
			ImGui::MenuItem("Show virtual keyboard", nullptr, &manager.keyboard.show);
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Advanced")) {
			ImGui::TextUnformatted("All settings");
			ImGui::Separator();
			auto& settingsManager = reactor.getGlobalCommandController().getSettingsManager();
			std::vector<Setting*> settings;
			for (auto* setting : settingsManager.getAllSettings()) {
				if (dynamic_cast<ProxySetting*>(setting)) continue;
				if (dynamic_cast<ReadOnlySetting*>(setting)) continue;
				settings.push_back(checked_cast<Setting*>(setting));
			}
			ranges::sort(settings, {}, &Setting::getBaseName);
			for (auto* setting : settings) {
				std::string name(setting->getBaseName());
				if (auto* bSetting = dynamic_cast<BooleanSetting*>(setting)) {
					Checkbox(name.c_str(), *bSetting);
				} else if (auto* iSetting = dynamic_cast<IntegerSetting*>(setting)) {
					SliderInt(name.c_str(), *iSetting);
				} else if (auto* fSetting = dynamic_cast<FloatSetting*>(setting)) {
					SliderFloat(name.c_str(), *fSetting);
				} else if (auto* sSetting = dynamic_cast<StringSetting*>(setting)) {
					InputText(name.c_str(), *sSetting);
				} else if (auto* fnSetting = dynamic_cast<FilenameSetting*>(setting)) {
					InputText(name.c_str(), *fnSetting); // TODO
				} else if (auto* kSetting = dynamic_cast<KeyCodeSetting*>(setting)) {
					InputText(name.c_str(), *kSetting); // TODO
				} else if (auto* eSetting = dynamic_cast<EnumSettingBase*>(setting)) {
					ComboBox(name.c_str(), *setting, *eSetting);
				} else if (auto* vSetting = dynamic_cast<VideoSourceSetting*>(setting)) {
					ComboBox(name.c_str(), *vSetting);
				} else {
					assert(false);
				}
			}
			ImGui::Separator();
			ImGui::Checkbox("ImGui Demo Window", &showDemoWindow);
			HelpMarker("Show the ImGui demo window.\n"
			           "This is purely to demonstrate the ImGui capabilities.\n"
			           "There is no connection with any openMSX functionality.");
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (showDemoWindow) {
		ImGui::ShowDemoWindow(&showDemoWindow);
	}
}

} // namespace openmsx
