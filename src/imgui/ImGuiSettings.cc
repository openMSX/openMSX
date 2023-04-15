#include "ImGuiSettings.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "Display.hh"
#include "FilenameSetting.hh"
#include "FloatSetting.hh"
#include "GlobalCommandController.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "KeyCodeSetting.hh"
#include "Mixer.hh"
#include "MSXMotherBoard.hh"
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

void ImGuiSettings::showMenu(MSXMotherBoard* motherBoard)
{
	if (ImGui::BeginMenu("Settings")) {
		auto& reactor = manager.getReactor();
		auto& globalSettings = reactor.getGlobalSettings();
		auto& renderSettings = reactor.getDisplay().getRenderSettings();
		auto& settingsManager = reactor.getGlobalCommandController().getSettingsManager();

		if (ImGui::BeginMenu("Video")) {
			if (ImGui::TreeNodeEx("Look and feel", ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& scaler = renderSettings.getScaleAlgorithmSetting();
				ComboBox(scaler);

				ImGui::Indent();
				struct AlgoEnable {
					RenderSettings::ScaleAlgorithm algo;
					bool hasScanline;
					bool hasBlur;
				};
				static constexpr std::array algoEnables = {
					//                                        scanline / blur
					AlgoEnable{RenderSettings::SCALER_SIMPLE,     true,  true },
					AlgoEnable{RenderSettings::SCALER_SCALE,      false, false},
					AlgoEnable{RenderSettings::SCALER_HQ,         false, false},
					AlgoEnable{RenderSettings::SCALER_HQLITE,     false, false},
					AlgoEnable{RenderSettings::SCALER_RGBTRIPLET, true,  true },
					AlgoEnable{RenderSettings::SCALER_TV,         true,  false},
				};
				auto it = ranges::find(algoEnables, scaler.getEnum(), &AlgoEnable::algo);
				assert(it != algoEnables.end());
				ImGui::BeginDisabled(!it->hasScanline);
				SliderInt(renderSettings.getScanlineSetting());
				ImGui::EndDisabled();
				ImGui::BeginDisabled(!it->hasBlur);
				SliderInt(renderSettings.getBlurSetting());
				ImGui::EndDisabled();
				ImGui::Unindent();

				SliderInt(renderSettings.getScaleFactorSetting());
				Checkbox(renderSettings.getDeinterlaceSetting());
				Checkbox(renderSettings.getDeflickerSetting());
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
				SliderFloat(renderSettings.getNoiseSetting());
				SliderFloat(renderSettings.getBrightnessSetting());
				SliderFloat(renderSettings.getContrastSetting());
				SliderFloat(renderSettings.getGammaSetting());
				SliderInt(renderSettings.getGlowSetting());
				if (auto* monitor = dynamic_cast<Setting*>(settingsManager.findSetting("monitor_type"))) {
					ComboBox(*monitor);
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
				SliderFloat(renderSettings.getHorizontalStretchSetting(), "%.0f");
				ComboBox(renderSettings.getDisplayDeformSetting());
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Misc", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (motherBoard) {
					ComboBox(motherBoard->getVideoSource());
				}
				Checkbox(renderSettings.getVSyncSetting());
				SliderInt(renderSettings.getMinFrameSkipSetting());
				SliderInt(renderSettings.getMaxFrameSkipSetting());
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Advanced (for debugging)")) { // default collapsed
				Checkbox(renderSettings.getLimitSpritesSetting());
				Checkbox(renderSettings.getDisableSpritesSetting());
				ComboBox(renderSettings.getTooFastAccessSetting());
				ComboBox(renderSettings.getCmdTimingSetting());
				ImGui::TreePop();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Sound")) {
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			ImGui::BeginDisabled(muteSetting.getBoolean());
			SliderInt("master volume", mixer.getMasterVolume());
			ImGui::EndDisabled();
			Checkbox(muteSetting);
			ImGui::Separator();
			static constexpr std::array resamplerToolTips = {
				EnumToolTip{"hq",   "best quality, uses more CPU"},
				EnumToolTip{"blip", "good speed/quality tradeoff"},
				EnumToolTip{"fast", "fast but low quality"},
			};
			ComboBox(globalSettings.getResampleSetting(), resamplerToolTips);
			ImGui::Separator();

			ImGui::MenuItem("Show sound chip settings", nullptr, &manager.soundChip.showSoundChipSettings);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Misc")) {
			auto& speedManager = globalSettings.getSpeedManager();
			auto& fwdSetting = speedManager.getFastForwardSetting();
			int fastForward = fwdSetting.getBoolean() ? 1 : 0;
			ImGui::TextUnformatted("Speed:");
			ImGui::SameLine();
			bool fwdChanged = ImGui::RadioButton("normal", &fastForward, 0);
			ImGui::SameLine();
			fwdChanged |= ImGui::RadioButton("fast forward", &fastForward, 1);
			HelpMarker("Use 'F9' to quickly toggle between these two");
			if (fwdChanged) {
				fwdSetting.setBoolean(fastForward != 0);
			}
			ImGui::Indent();
			ImGui::BeginDisabled(fastForward != 0);
			SliderInt(speedManager.getSpeedSetting());
			ImGui::EndDisabled();
			ImGui::BeginDisabled(fastForward != 1);
			SliderInt(speedManager.getFastForwardSpeedSetting());
			ImGui::EndDisabled();
			ImGui::Unindent();
			Checkbox(globalSettings.getThrottleManager().getFullSpeedLoadingSetting());

			ImGui::Separator();
			ImGui::MenuItem("Show OSD icons", nullptr, &manager.osdIcons.showIcons);
			ImGui::MenuItem("Show virtual keyboard", nullptr, &manager.keyboard.show);
			ImGui::MenuItem("Show console", "F10", &manager.console.show);
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Advanced")) {
			ImGui::TextUnformatted("All settings");
			ImGui::Separator();
			std::vector<Setting*> settings;
			for (auto* setting : settingsManager.getAllSettings()) {
				if (dynamic_cast<ProxySetting*>(setting)) continue;
				if (dynamic_cast<ReadOnlySetting*>(setting)) continue;
				settings.push_back(checked_cast<Setting*>(setting));
			}
			ranges::sort(settings, {}, &Setting::getBaseName);
			for (auto* setting : settings) {
				if (auto* bSetting = dynamic_cast<BooleanSetting*>(setting)) {
					Checkbox(*bSetting);
				} else if (auto* iSetting = dynamic_cast<IntegerSetting*>(setting)) {
					SliderInt(*iSetting);
				} else if (auto* fSetting = dynamic_cast<FloatSetting*>(setting)) {
					SliderFloat(*fSetting);
				} else if (auto* sSetting = dynamic_cast<StringSetting*>(setting)) {
					InputText(*sSetting);
				} else if (auto* fnSetting = dynamic_cast<FilenameSetting*>(setting)) {
					InputText(*fnSetting); // TODO
				} else if (auto* kSetting = dynamic_cast<KeyCodeSetting*>(setting)) {
					InputText(*kSetting); // TODO
				} else if (dynamic_cast<EnumSettingBase*>(setting)) {
					ComboBox(*setting);
				} else if (auto* vSetting = dynamic_cast<VideoSourceSetting*>(setting)) {
					ComboBox(*vSetting);
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
