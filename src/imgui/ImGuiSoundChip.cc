#include "ImGuiSoundChip.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"
#include "StringSetting.hh"

#include <imgui.h>

#include <algorithm>

using namespace std::literals;


namespace openmsx {

void ImGuiSoundChip::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& [name, enabled] : channels) {
		buf.appendf("showChannels.%s=%d\n", name.c_str(), enabled);
	}
}

void ImGuiSoundChip::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name.starts_with("showChannels.")) {
		channels[std::string(name.substr(13))] = StringOp::stringToBool(value);
	}
}

void ImGuiSoundChip::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;
	if (showSoundChipSettings) {
		showChipSettings(*motherBoard);
	}

	// Show sound chip channel settings
	const auto& msxMixer = motherBoard->getMSXMixer();
	const auto& infos = msxMixer.getDeviceInfos();
	for (const auto& info : infos) {
		const auto& name = info.device->getName();
		auto [it, inserted] = channels.try_emplace(name, false);
		auto& enabled = it->second;
		if (enabled) {
			showChannelSettings(*motherBoard, name, &enabled);
		}
	}
}

[[nodiscard]] static bool anySpecialChannelSettings(const MSXMixer::SoundDeviceInfo& info)
{
	return std::ranges::any_of(info.channelSettings, [&](const auto& channel) {
		return channel.mute->getBoolean() || !channel.record->getString().empty();
	});
}

void ImGuiSoundChip::showChipSettings(MSXMotherBoard& motherBoard)
{
	auto restoreDefaultPopup = [](const char* label, Setting& setting) {
		im::PopupContextItem([&]{
			if (ImGui::Button(label)) {
				setting.setValue(setting.getDefaultValue());
				ImGui::CloseCurrentPopup();
			}
		});
	};

	im::Window("Sound chip settings", &showSoundChipSettings, [&]{
		int tableFlags = ImGuiTableFlags_RowBg |
		                 ImGuiTableFlags_BordersV |
		                 ImGuiTableFlags_BordersOuter |
		                 ImGuiTableFlags_Reorderable |
		                 ImGuiTableFlags_Hideable |
		                 ImGuiTableFlags_NoHostExtendX;
		im::Table("table", 4, tableFlags, [&]{
			ImGui::TableSetupColumn("Device",   ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Volume",   ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 10.0f * ImGui::GetFontSize());
			ImGui::TableSetupColumn("Balance",  ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,  6.0f * ImGui::GetFontSize());
			ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,  5.0f * ImGui::GetFontSize());
			ImGui::TableHeadersRow();

			const auto& msxMixer = motherBoard.getMSXMixer();
			const auto& infos = msxMixer.getDeviceInfos(); // TODO sort on name
			im::ID_for_range(infos.size(), [&](int i) {
				const auto& info = infos[i];
				if (ImGui::TableNextColumn()) {
					const auto& device = *info.device;
					ImGui::TextUnformatted(device.getName());
					simpleToolTip(device.getDescription());
				}
				if (ImGui::TableNextColumn()) {
					auto& volumeSetting = *info.volumeSetting;
					int volume = volumeSetting.getInt();
					int min = volumeSetting.getMinValue();
					int max = volumeSetting.getMaxValue();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::SliderInt("##volume", &volume, min, max)) {
						volumeSetting.setInt(volume);
					}
					restoreDefaultPopup("Set default", volumeSetting);
				}
				if (ImGui::TableNextColumn()) {
					auto& balanceSetting = *info.balanceSetting;
					int balance = balanceSetting.getInt();
					int min = balanceSetting.getMinValue();
					int max = balanceSetting.getMaxValue();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::SliderInt("##balance", &balance, min, max)) {
						balanceSetting.setInt(balance);
					}
					restoreDefaultPopup("Set center", balanceSetting);
				}
				if (ImGui::TableNextColumn()) {
					bool special = anySpecialChannelSettings(info);
					if (special) {
						ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, getColor(imColor::YELLOW));
					}
					const auto& name = info.device->getName();
					auto [it, inserted] = channels.try_emplace(name, false);
					auto& enabled = it->second;
					im::Indent([&]{
						ImGui::Checkbox("##channels", &enabled);
					});
				}
			});
		});
	});
}

void ImGuiSoundChip::showChannelSettings(MSXMotherBoard& motherBoard, const std::string& name, bool* enabled)
{
	const auto& msxMixer = motherBoard.getMSXMixer();
	const auto* info = msxMixer.findDeviceInfo(name);
	if (!info) return;

	auto label = tmpStrCat(name, " channel settings");
	ImGui::SetNextWindowSize(gl::vec2{40, 0} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window(label.c_str(), enabled, [&]{
		const auto& hotKey = manager.getReactor().getHotKey();
		int flags = ImGuiTableFlags_ScrollY |
			    ImGuiTableFlags_RowBg |
		            ImGuiTableFlags_SizingStretchProp;
		im::Table("table", 3, flags, [&]{
			ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
			ImGui::TableSetupColumn("ch.",  ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("mute", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("file to record to", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			im::ID_for_range(info->channelSettings.size(), [&](int i) {
				const auto& channel =  info->channelSettings[i];
				if (ImGui::TableNextColumn()) {
					ImGui::StrCat(i + 1);
				}
				if (ImGui::TableNextColumn()) {
					Checkbox(hotKey, "##mute", *channel.mute);
				}
				if (ImGui::TableNextColumn()) {
					// TODO: use a file browser (in "create mode")
					ImGui::SetNextItemWidth(-FLT_MIN);
					InputText("##rec", *channel.record);
				}
			});
		});
	});
}

} // namespace openmsx
