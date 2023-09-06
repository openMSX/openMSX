#include "ImGuiSettings.hh"

#include "ImGuiCpp.hh"
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
#include "StringOp.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

using namespace std::literals;

namespace openmsx {

void ImGuiSettings::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Settings", [&]{
		auto& reactor = manager.getReactor();
		auto& globalSettings = reactor.getGlobalSettings();
		auto& renderSettings = reactor.getDisplay().getRenderSettings();
		auto& settingsManager = reactor.getGlobalCommandController().getSettingsManager();

		im::Menu("Video", [&]{
			im::TreeNode("Look and feel", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto& scaler = renderSettings.getScaleAlgorithmSetting();
				ComboBox("Scaler", scaler);
				im::Indent([&]{
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
					im::Disabled(!it->hasScanline, [&]{
						SliderInt("Scanline (%)", renderSettings.getScanlineSetting());
					});
					im::Disabled(!it->hasBlur, [&]{
						SliderInt("Blur (%)", renderSettings.getBlurSetting());
					});
				});

				SliderInt("Scale factor", renderSettings.getScaleFactorSetting());
				Checkbox("Deinterlace", renderSettings.getDeinterlaceSetting());
				Checkbox("Deflicker", renderSettings.getDeflickerSetting());
			});
			im::TreeNode("Colors", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				SliderFloat("Noise (%)", renderSettings.getNoiseSetting());
				SliderFloat("Brightness", renderSettings.getBrightnessSetting());
				SliderFloat("Contrast", renderSettings.getContrastSetting());
				SliderFloat("Gamma", renderSettings.getGammaSetting());
				SliderInt("Glow (%)", renderSettings.getGlowSetting());
				if (auto* monitor = dynamic_cast<Setting*>(settingsManager.findSetting("monitor_type"))) {
					ComboBox("Monitor type", *monitor, [](std::string s) {
						ranges::replace(s, '_', ' ');
						return s;
					});
				}
			});
			im::TreeNode("Shape", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				SliderFloat("Horizontal stretch", renderSettings.getHorizontalStretchSetting(), "%.0f");
				ComboBox("Display deformation", renderSettings.getDisplayDeformSetting());
			});
			im::TreeNode("Misc", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				if (motherBoard) {
					ComboBox("Video source to display", motherBoard->getVideoSource());
				}
				Checkbox("VSync", renderSettings.getVSyncSetting());
				SliderInt("Minimum frame-skip", renderSettings.getMinFrameSkipSetting()); // TODO: either leave out this setting, or add a tooltip like, "Leave on 0 unless you use a very slow device and want regular frame skipping");
				SliderInt("Maximum frame-skip", renderSettings.getMaxFrameSkipSetting()); // TODO: either leave out this setting or add a tooltip like  "On slow devices, skip no more than this amount of frames to keep emulation on time.");
			});
			im::TreeNode("Advanced (for debugging)", [&]{ // default collapsed
				Checkbox("Enforce VDP sprites-per-line limit", renderSettings.getLimitSpritesSetting());
				Checkbox("Disable sprites", renderSettings.getDisableSpritesSetting());
				ComboBox("Way to handle too fast VDP access", renderSettings.getTooFastAccessSetting());
				ComboBox("Emulate VDP command timing", renderSettings.getCmdTimingSetting());
			});
		});
		im::Menu("Sound", [&]{
			auto& mixer = reactor.getMixer();
			auto& muteSetting = mixer.getMuteSetting();
			im::Disabled(muteSetting.getBoolean(), [&]{
				SliderInt("Master volume", mixer.getMasterVolume());
			});
			Checkbox("Mute", muteSetting);
			ImGui::Separator();
			static constexpr std::array resamplerToolTips = {
				EnumToolTip{"hq",   "best quality, uses more CPU"},
				EnumToolTip{"blip", "good speed/quality tradeoff"},
				EnumToolTip{"fast", "fast but low quality"},
			};
			ComboBox("Resampler", globalSettings.getResampleSetting(), resamplerToolTips);
			ImGui::Separator();

			ImGui::MenuItem("Show sound chip settings", nullptr, &manager.soundChip.showSoundChipSettings);
		});
		im::Menu("Misc", [&]{
			auto& speedManager = globalSettings.getSpeedManager();
			auto& fwdSetting = speedManager.getFastForwardSetting();
			int fastForward = fwdSetting.getBoolean() ? 1 : 0;
			ImGui::TextUnformatted("Speed:"sv);
			ImGui::SameLine();
			bool fwdChanged = ImGui::RadioButton("normal", &fastForward, 0);
			ImGui::SameLine();
			fwdChanged |= ImGui::RadioButton("fast forward", &fastForward, 1);
			HelpMarker("Use 'F9' to quickly toggle between these two"); // TODO: this depends on bind configuration, check it...
			if (fwdChanged) {
				fwdSetting.setBoolean(fastForward != 0);
			}
			im::Indent([&]{
				im::Disabled(fastForward != 0, [&]{
					SliderInt("Speed (%)", speedManager.getSpeedSetting());
				});
				im::Disabled(fastForward != 1, [&]{
					SliderInt("Fast forward speed (%)", speedManager.getFastForwardSpeedSetting());
				});
			});
			Checkbox("Go full speed when loading", globalSettings.getThrottleManager().getFullSpeedLoadingSetting());

			ImGui::Separator();
			if (ImGui::MenuItem("Configure OSD icons...", nullptr, nullptr)) {
				manager.osdIcons.showConfigureIcons = true;
			}
			ImGui::MenuItem("Fade out menu bar", nullptr, &manager.menuFade);
			ImGui::MenuItem("Configure messages ...", nullptr, &manager.messages.showConfigure);
		});
		ImGui::Separator();
		im::Menu("Advanced", [&]{
			ImGui::TextUnformatted("All settings"sv);
			ImGui::Separator();
			std::vector<Setting*> settings;
			for (auto* setting : settingsManager.getAllSettings()) {
				if (dynamic_cast<ProxySetting*>(setting)) continue;
				if (dynamic_cast<ReadOnlySetting*>(setting)) continue;
				settings.push_back(checked_cast<Setting*>(setting));
			}
			ranges::sort(settings, StringOp::caseless{}, &Setting::getBaseName);
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
		});
	});
	if (showDemoWindow) {
		ImGui::ShowDemoWindow(&showDemoWindow);
	}
}

} // namespace openmsx
