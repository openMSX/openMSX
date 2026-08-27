#include "ImGuiIoPorts.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "DummyDevice.hh"
#include "EmuTime.hh"
#include "HardwareConfig.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXMultiIODevice.hh"

#include "join.hh"
#include "narrow.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

using namespace std::literals;

static constexpr auto OVERLAP_TOOLTIP =
	"These ports are shared with another device. Reading such a port returns "
	"the bitwise AND of all values, writing goes to every device."sv;

static constexpr int TABLE_FLAGS =
	ImGuiTableFlags_RowBg |
	ImGuiTableFlags_BordersInnerV |
	ImGuiTableFlags_Resizable |
	ImGuiTableFlags_Reorderable |
	ImGuiTableFlags_Hideable |
	ImGuiTableFlags_ContextMenuInBody;

/** A maximal range of ports with the same device(s) in the same direction(s).
  */
struct Row {
	uint8_t begin; // inclusive
	uint8_t end;   // inclusive
	bool in;
	bool out;
	bool overlap;       // these ports are shared with another device
	std::string device; // comma separated name(s)
};

using Devices = MSXMultiIODevice::Devices;

// Collect the device(s) on one port. This mirrors 'machine_info
// input_port/output_port', on which the 'iomap' script is built: an unused
// port yields an empty list, a port with conflicting devices one entry per
// device.
[[nodiscard]] static Devices getDevices(MSXCPUInterface& cpuInterface, uint8_t port, bool isIn)
{
	auto* device = cpuInterface.getIODevice(port, isIn);
	const MSXDevice* dummy = &cpuInterface.getDummyDevice();
	if (device == dummy) return {}; // unused port
	if (auto* multi = dynamic_cast<MSXMultiIODevice*>(device)) {
		// A sub-device is never the DummyDevice: register_IO() only wraps
		// devices in a MSXMultiIODevice once a real device is registered,
		// and unregister_IO() unwraps again when one device is left.
		return multi->getDevices();
	}
	return {device};
}

struct PortDevices {
	std::array<Devices, 256> in;
	std::array<Devices, 256> out;
};
[[nodiscard]] static PortDevices collectDevices(MSXCPUInterface& cpuInterface)
{
	PortDevices result;
	for (auto port : xrange(256)) {
		result.in [port] = getDevices(cpuInterface, narrow<uint8_t>(port), true);
		result.out[port] = getDevices(cpuInterface, narrow<uint8_t>(port), false);
	}
	return result;
}

[[nodiscard]] static std::string portsText(const Row& row)
{
	if (row.begin == row.end) {
		return strCat(hex_string<2, HexCase::upper>(row.begin));
	}
	return strCat(hex_string<2, HexCase::upper>(row.begin), '-',
	              hex_string<2, HexCase::upper>(row.end));
}

[[nodiscard]] static std::string_view dirText(const Row& row)
{
	return row.in ? (row.out ? "I/O"sv : "I"sv) : "O"sv;
}

static void sortRows(std::vector<Row>& rows)
{
	// Note: unlike the other views in openMSX, 'rows' is rebuilt from scratch
	// every frame. So (re)apply the sort unconditionally instead of only when
	// 'SpecsDirty' is set.
	const auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs || (sortSpecs->SpecsCount == 0)) return;
	assert(sortSpecs->Specs);

	bool descending = sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending;
	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // ports: 'rows' is already generated in ascending port order
		if (descending) std::ranges::reverse(rows);
		break;
	case 2: // device: stable, so each device keeps its ports port-ordered
		sortUpDown_String(rows, sortSpecs, &Row::device);
		break;
	default:
		// 'Dir' and 'Value' are NoSort, but Dear ImGui still restores a sort
		// on such a column from imgui.ini (TableSortSpecsSanitize() only
		// clears it for hidden columns). So don't assume this can't happen,
		// just keep the rows in ascending port order.
		break;
	}
}

[[nodiscard]] static std::vector<Row> getRows(MSXCPUInterface& cpuInterface)
{
	auto devices = collectDevices(cpuInterface);
	const auto& in = devices.in;
	const auto& out = devices.out;

	std::vector<Row> result;
	unsigned port = 0;
	while (port < 256) {
		// coalesce consecutive ports with identical input and output device(s)
		unsigned end = port + 1;
		while ((end < 256) && (in[end] == in[port]) && (out[end] == out[port])) {
			++end;
		}
		auto add = [&](const Devices& devs, bool isIn, bool isOut) {
			if (devs.empty()) return;
			result.push_back(Row{
				.begin = narrow<uint8_t>(port),
				.end = narrow<uint8_t>(end - 1),
				.in = isIn,
				.out = isOut,
				.overlap = devs.size() > 1,
				.device = join(std::views::transform(devs, &MSXDevice::getName), ", ")});
		};
		if (in[port] == out[port]) {
			add(in[port], true, true);
		} else {
			add(in [port], true, false);
			add(out[port], false, true);
		}
		port = end;
	}
	return result;
}

static void drawValue(ImFont* fontMono, MSXCPUInterface& cpuInterface,
                      const Row& row, EmuTime time)
{
	im::ScopedFont sf(fontMono);
	if (!row.in) {
		// Peeking would read the input side, which has nothing to do with
		// the (output-only) device on this row.
		ImGui::TextUnformatted("-"sv);
		return;
	}
	// Draw all values and let Dear ImGui clip them. Only the gfx9000 has more
	// than a handful of ports, and this column is hidden by default anyway.
	std::string values;
	for (auto port : xrange(unsigned(row.begin), unsigned(row.end) + 1)) {
		if (!values.empty()) strAppend(values, ' ');
		strAppend(values, hex_string<2, HexCase::upper>(
			cpuInterface.peekIO(narrow<uint16_t>(port), time)));
	}
	ImGui::TextUnformatted(values);
}

static void drawTable(ImFont* fontMono, MSXCPUInterface& cpuInterface,
                      bool warnOverlap, EmuTime time)
{
	// 'Ports' and 'Dir' have a fixed width, so it must fit both the content
	// and the header. 'Ports' is sortable, and a sorted column also draws a
	// sort arrow next to its name, otherwise that header would be clipped to
	// "..." as soon as you sort on it.
	const auto& style = ImGui::GetStyle();
	auto textWidth = [](std::string_view s) { return ImGui::CalcTextSize(s).x; };
	auto monoWidth = [&](std::string_view s) {
		im::ScopedFont sf(fontMono);
		return ImGui::CalcTextSize(s).x;
	};
	auto columnWidth = [&](float header, float content) {
		return std::max(header, content) + 2.0f * style.CellPadding.x;
	};
	auto arrowWidth = ImGui::GetFontSize() + style.ItemInnerSpacing.x;

	im::Table("ports", 4, TABLE_FLAGS | ImGuiTableFlags_Sortable, [&]{
		// 'Value' is hidden by default: the hex editor on the 'ioports'
		// debuggable already shows these values. Un-hide it via the
		// right-click context menu on the table header.
		// It's also not sortable: the values change while the emulation
		// runs, so sorting on them would reshuffle the rows every frame.
		//
		// 'Ports' and 'Dir' hold at most 5 resp. 3 characters, so resizing
		// them can only take space away from the other two. Those stretch
		// instead, so widening the window widens both.
		ImGui::TableSetupColumn("Ports", ImGuiTableColumnFlags_WidthFixed |
		                                 ImGuiTableColumnFlags_NoResize |
		                                 ImGuiTableColumnFlags_DefaultSort,
		                        columnWidth(textWidth("Ports"sv) + arrowWidth,
		                                    monoWidth("FF-FF"sv)));
		ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed |
		                               ImGuiTableColumnFlags_NoResize |
		                               ImGuiTableColumnFlags_NoSort,
		                        columnWidth(textWidth("Dir"sv), textWidth("I/O"sv)));
		ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch |
		                                 ImGuiTableColumnFlags_DefaultHide |
		                                 ImGuiTableColumnFlags_NoSort);
		ImGui::TableHeadersRow();

		auto rows = getRows(cpuInterface);
		sortRows(rows);
		for (const auto& row : rows) {
			if (ImGui::TableNextColumn()) { // ports
				im::ScopedFont sf(fontMono);
				ImGui::TextUnformatted(portsText(row));
			}
			if (ImGui::TableNextColumn()) { // direction
				ImGui::TextUnformatted(dirText(row));
			}
			if (ImGui::TableNextColumn()) { // device(s)
				bool conflict = row.overlap && warnOverlap;
				im::StyleColor(conflict, ImGuiCol_Text, getColor(imColor::YELLOW), [&]{
					ImGui::TextUnformatted(row.device);
				});
				if (conflict) simpleToolTip(OVERLAP_TOOLTIP);
			}
			if (ImGui::TableNextColumn()) { // value(s)
				drawValue(fontMono, cpuInterface, row, time);
			}
		}
	});
}

void ImGuiIoPorts::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiIoPorts::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiIoPorts::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	// Machines can explicitly declare that overlapping I/O ports are
	// intentional, in that case don't flag them.
	const auto* config = motherBoard->getMachineConfig();
	bool warnOverlap = !config ||
		config->getDevicesElem().getAttributeValueAsBool("overlap_warning", true);

	auto fontSize = ImGui::GetFontSize();
	ImGui::SetNextWindowSize(ImVec2(46.0f * fontSize, 26.0f * fontSize), ImGuiCond_FirstUseEver);
	im::Window("I/O ports", &show, [&]{
		// A device doesn't necessarily occupy a contiguous range of ports
		// (e.g. the Philips NMS-1205). Sort on 'Device' to see all ports of
		// one device together instead of ordered by port number.
		drawTable(manager.fontMono, motherBoard->getCPUInterface(),
		          warnOverlap, motherBoard->getCurrentTime());
	});
}

} // namespace openmsx
