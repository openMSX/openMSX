#include "ImGuiSCCViewer.hh"
#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "SoundDevice.hh"
#include "MSXMixer.hh"
#include "SCC.hh"

namespace openmsx {

ImGuiSCCViewer::ImGuiSCCViewer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
}


void paintSCC(const SCC& scc);

void ImGuiSCCViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	im::Window("SCC Viewer", &show, [&] {
		bool noDevices = true;
		for (auto& info: motherBoard->getMSXMixer().getDeviceInfos()) {
			if (auto* device = dynamic_cast<SCC*>(info.device)) {
				noDevices = false;
				paintSCC(*device);
			}
		}
		if (noDevices) {
			ImGui::TextUnformatted("No SCC devices currently present in the system.");
		}

	});
}

void paintSCC(const SCC& scc) {
	im::TreeNode(scc.getName().c_str(), ImGuiTreeNodeFlags_DefaultOpen, [&] {
		const auto& waveData = scc.getWaveData();
		for (auto [channelNr, channelWaveData] : enumerate(waveData)) {
			auto getFloatData = [](void* data, int idx) {
				return float(static_cast<const int8_t*>(data)[idx]);
			};
			const auto scale = 2.0f;
			ImGui::PlotHistogram("",
				getFloatData,
				const_cast<int8_t*>(channelWaveData.data()),
				channelWaveData.size(),
				0, nullptr,
				-128.0f, 127.0f,
				{32.0f * scale + 2.0f * ImGui::GetStyle().FramePadding.x, 64.0f * scale + 2.0f * ImGui::GetStyle().FramePadding.y});
			if (channelNr < (waveData.size() - 1)) ImGui::SameLine();
		}
	});
}

} // namespace openmsx
