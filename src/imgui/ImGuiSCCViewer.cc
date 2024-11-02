#include "ImGuiSCCViewer.hh"

#include "MSXDevice.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SCC.hh"
#include "SoundDevice.hh"

#include "narrow.hh"

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
			if (const auto* device = dynamic_cast<const SCC*>(info.device)) {
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
			auto size = scale * gl::vec2{32.0f, 64.0f} + 2.0f * gl::vec2(ImGui::GetStyle().FramePadding);
			ImGui::PlotHistogram(tmpStrCat("##sccWave", channelNr).c_str(),
				getFloatData,
				const_cast<int8_t*>(channelWaveData.data()),
				narrow<int>(channelWaveData.size()),
				0, nullptr,
				-128.0f, 127.0f,
				size);
			if (channelNr < (waveData.size() - 1)) ImGui::SameLine();
		}
	});
}

} // namespace openmsx
