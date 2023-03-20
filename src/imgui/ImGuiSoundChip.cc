#include "ImGuiSoundChip.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"
#include "StringSetting.hh"

#include <imgui.h>

namespace openmsx {

void ImGuiSoundChip::save(ImGuiTextBuffer& buf)
{
	buf.append("[openmsx][sound chip settings]\n");
	buf.appendf("show=%d\n", showSoundChipSettings);
	for (const auto& [name, enabled] : channels) {
		buf.appendf("showChannels.%s=%d\n", name.c_str(), enabled);
	}
	buf.append("\n");
}

void ImGuiSoundChip::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		showSoundChipSettings = StringOp::stringToBool(value);
	} else if (name.starts_with("showChannels.")) {
		channels[std::string(name.substr(13))] = StringOp::stringToBool(value);
	}
}

void ImGuiSoundChip::paint(MSXMotherBoard& motherBoard)
{
	if (showSoundChipSettings) {
		showChipSettings(motherBoard);
	}

	// Show sound chip channel settings
	auto& msxMixer = motherBoard.getMSXMixer();
	auto& infos = msxMixer.getDeviceInfos();
	for (const auto& info : infos) {
		const auto& name = info.device->getName();
		auto [it, inserted] = channels.try_emplace(name, false);
		auto& enabled = it->second;
		if (enabled) {
			showChannelSettings(motherBoard, name, &enabled);
		}
	}
}

static bool anySpecialChannelSettings(const MSXMixer::SoundDeviceInfo& info)
{
	for (const auto& channel : info.channelSettings) {
		if (channel.mute->getBoolean()) return true;
		if (!channel.record->getString().empty()) return true;
	}
	return false;
}

void ImGuiSoundChip::showChipSettings(MSXMotherBoard& motherBoard)
{
	auto restoreDefaultPopup = [](const char* label, Setting& setting) {
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::Button(label)) {
				setting.setValue(setting.getDefaultValue());
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	};

	ImGui::Begin("Sound chip settings", &showSoundChipSettings);

	auto& msxMixer = motherBoard.getMSXMixer();
	auto& infos = msxMixer.getDeviceInfos(); // TODO sort on name
	if (ImGui::BeginTable("table", narrow<int>(infos.size()), ImGuiTableFlags_ScrollX)) {
		for (auto& info : infos) {
			auto& device = *info.device;
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(device.getName().c_str());
			simpleToolTip(device.getDescription());
		}
		for (auto& info : infos) {
			auto& volumeSetting = *info.volumeSetting;
			int volume = volumeSetting.getInt();
			int min = volumeSetting.getMinValue();
			int max = volumeSetting.getMaxValue();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("volume");
			std::string id = "##volume-" + info.device->getName();
			if (ImGui::VSliderInt(id.c_str(), ImVec2(18, 120), &volume, min, max)) {
				volumeSetting.setInt(volume);
			}
			restoreDefaultPopup("Set default", volumeSetting);
		}
		for (auto& info : infos) {
			auto& balanceSetting = *info.balanceSetting;
			int balance = balanceSetting.getInt();
			int min = balanceSetting.getMinValue();
			int max = balanceSetting.getMaxValue();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("balance");
			std::string id = "##balance-" + info.device->getName();
			if (ImGui::SliderInt(id.c_str(), &balance, min, max)) {
				balanceSetting.setInt(balance);
			}
			restoreDefaultPopup("Set center", balanceSetting);
		}
		for (auto& info : infos) {
			ImGui::TableNextColumn();
			if (anySpecialChannelSettings(info)) {
				ImU32 color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 0.75f));
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color);
			}
			ImGui::TextUnformatted("channels");
			const auto& name = info.device->getName();
			std::string id = "##channels-" + name;
			auto [it, inserted] = channels.try_emplace(name, false);
			auto& enabled = it->second;
			ImGui::Checkbox(id.c_str(), &enabled);
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiSoundChip::showChannelSettings(MSXMotherBoard& motherBoard, const std::string& name, bool* enabled)
{
	auto& msxMixer = motherBoard.getMSXMixer();
	auto* info = msxMixer.findDeviceInfo(name);
	if (!info) return;

	std::string label = name + " channel setting";
	if (ImGui::Begin(label.c_str(), enabled)) {
		if (ImGui::BeginTable("table", 3)) {
			int counter = 0;
			for (auto& channel : info->channelSettings) {
				ImGui::PushID(counter);
				++counter;
				ImGui::TableNextColumn();
				ImGui::Text("channel %d", counter);

				ImGui::TableNextColumn();
				Checkbox("mute", *channel.mute);

				ImGui::TableNextColumn();
				InputText("record", *channel.record);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

} // namespace openmsx
