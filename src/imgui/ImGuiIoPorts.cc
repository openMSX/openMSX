#include "ImGuiIoPorts.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "HardwareConfig.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "MSXMultiIODevice.hh"

#include "narrow.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <string_view>

namespace openmsx {

using namespace std::literals;

// Only this many port values are shown inline, the rest is in the tooltip.
static constexpr unsigned MAX_SHOWN_VALUES = 8;

static constexpr auto OVERLAP_TOOLTIP =
	"These ports are shared with another device. Reading such a port returns "
	"the bitwise AND of all values, writing goes to every device."sv;

static constexpr int TABLE_FLAGS =
	ImGuiTableFlags_BordersInnerV |
	ImGuiTableFlags_Resizable |
	ImGuiTableFlags_Reorderable |
	ImGuiTableFlags_Hideable |
	ImGuiTableFlags_ContextMenuInBody;

using Names = std::vector<std::string_view>;

// Collect the name(s) of the device(s) on one port. This mirrors
// 'machine_info input_port/output_port', on which the 'iomap' script is
// built: an unused port yields an empty list (the DummyDevice has an empty
// name), a port with conflicting devices yields one name per device.
[[nodiscard]] static Names getNames(MSXCPUInterface& cpuInterface, uint8_t port, bool isIn)
{
	Names result;
	auto add = [&](const std::string& name) {
		if (!name.empty()) result.emplace_back(name);
	};
	auto* device = cpuInterface.getIODevice(port, isIn);
	if (auto* multi = dynamic_cast<MSXMultiIODevice*>(device)) {
		for (const auto* dev : multi->getDevices()) {
			add(dev->getName());
		}
	} else {
		add(device->getName());
	}
	return result;
}

struct PortNames {
	std::array<Names, 256> in;
	std::array<Names, 256> out;
};
[[nodiscard]] static PortNames collectNames(MSXCPUInterface& cpuInterface)
{
	PortNames result;
	for (auto port : xrange(256)) {
		result.in [port] = getNames(cpuInterface, narrow<uint8_t>(port), true);
		result.out[port] = getNames(cpuInterface, narrow<uint8_t>(port), false);
	}
	return result;
}

[[nodiscard]] static std::string join(const Names& names)
{
	std::string result;
	for (const auto& name : names) {
		if (!result.empty()) strAppend(result, ", ");
		strAppend(result, name);
	}
	return result;
}

std::string ImGuiIoPorts::portsText(const Row& row)
{
	if (row.begin == row.end) {
		return strCat(hex_string<2, HexCase::upper>(row.begin));
	}
	return strCat(hex_string<2, HexCase::upper>(row.begin), '-',
	              hex_string<2, HexCase::upper>(row.end));
}

std::string_view ImGuiIoPorts::dirText(const Row& row)
{
	return row.in ? (row.out ? "I/O"sv : "I"sv) : "O"sv;
}

std::vector<ImGuiIoPorts::Row> ImGuiIoPorts::getRows(MSXCPUInterface& cpuInterface)
{
	auto names = collectNames(cpuInterface);
	const auto& in = names.in;
	const auto& out = names.out;

	std::vector<Row> result;
	unsigned port = 0;
	while (port < 256) {
		// coalesce consecutive ports with identical input and output device(s)
		unsigned end = port + 1;
		while ((end < 256) && (in[end] == in[port]) && (out[end] == out[port])) {
			++end;
		}
		auto add = [&](const Names& n, bool isIn, bool isOut) {
			if (n.empty()) return;
			result.push_back(Row{
				.begin = narrow<uint8_t>(port),
				.end = narrow<uint8_t>(end - 1),
				.in = isIn,
				.out = isOut,
				.overlap = n.size() > 1,
				.device = join(n)});
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

std::vector<ImGuiIoPorts::Group> ImGuiIoPorts::getGroups(MSXCPUInterface& cpuInterface)
{
	auto names = collectNames(cpuInterface);
	const auto& in = names.in;
	const auto& out = names.out;

	// Per device, which ports it occupies (bit 0 = input, bit 1 = output).
	struct Acc {
		std::string_view device;
		std::array<uint8_t, 256> mask = {};
	};
	std::vector<Acc> accs;
	auto mark = [&](std::string_view device, unsigned port, uint8_t bit) {
		auto it = std::ranges::find(accs, device, &Acc::device);
		if (it == accs.end()) {
			accs.push_back(Acc{.device = device});
			it = accs.end() - 1;
		}
		it->mask[port] |= bit;
	};
	// Ports are visited in ascending order, so the devices end up ordered by
	// their lowest port. That keeps the grouped view roughly address ordered.
	for (auto port : xrange(256)) {
		for (const auto& device : in [port]) mark(device, port, 1);
		for (const auto& device : out[port]) mark(device, port, 2);
	}

	std::vector<Group> result;
	for (const auto& acc : accs) {
		Group group{.device = std::string(acc.device)};
		unsigned port = 0;
		while (port < 256) {
			if (acc.mask[port] == 0) { ++port; continue; }
			unsigned end = port + 1;
			while ((end < 256) && (acc.mask[end] == acc.mask[port])) ++end;
			bool isIn  = (acc.mask[port] & 1) != 0;
			bool isOut = (acc.mask[port] & 2) != 0;
			bool shared = std::ranges::any_of(xrange(port, end), [&](unsigned p) {
				return (isIn  && (in [p].size() > 1)) ||
				       (isOut && (out[p].size() > 1));
			});
			group.ranges.push_back(Row{
				.begin = narrow<uint8_t>(port),
				.end = narrow<uint8_t>(end - 1),
				.in = isIn,
				.out = isOut,
				.overlap = shared});
			port = end;
		}
		result.push_back(std::move(group));
	}
	return result;
}

void ImGuiIoPorts::drawValue(MSXCPUInterface& cpuInterface, const Row& row, EmuTime time)
{
	im::ScopedFont sf(manager.fontMono);
	if (!row.in) {
		// Peeking would read the input side, which has nothing to do with
		// the (output-only) device on this row.
		ImGui::TextUnformatted("-"sv);
		return;
	}
	auto num = unsigned(row.end) - row.begin + 1;
	std::string values;
	for (auto i : xrange(num)) {
		if (i) strAppend(values, ' ');
		strAppend(values, hex_string<2, HexCase::upper>(
			cpuInterface.peekIO(narrow<uint16_t>(row.begin + i), time)));
	}
	if (num <= MAX_SHOWN_VALUES) {
		ImGui::TextUnformatted(values);
	} else {
		// every value takes 3 characters ("XX "), minus the trailing space
		ImGui::StrCat(std::string_view(values).substr(0, MAX_SHOWN_VALUES * 3 - 1), " ...");
		simpleToolTip(values);
	}
}

void ImGuiIoPorts::drawFlat(MSXCPUInterface& cpuInterface, bool warnOverlap, EmuTime time)
{
	im::Table("flat", 4, TABLE_FLAGS, [&]{
		// 'Value' is hidden by default: the hex editor on the 'ioports'
		// debuggable already shows these values. Un-hide it via the
		// right-click context menu on the table header.
		ImGui::TableSetupColumn("Ports", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed |
		                                 ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();

		for (const auto& row : getRows(cpuInterface)) {
			if (ImGui::TableNextColumn()) { // ports
				im::ScopedFont sf(manager.fontMono);
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
				drawValue(cpuInterface, row, time);
			}
		}
	});
}

void ImGuiIoPorts::drawGrouped(MSXCPUInterface& cpuInterface, bool warnOverlap, EmuTime time)
{
	// Size to content: the tree column is the first one, stretching it would
	// push 'Dir' all the way to the right edge.
	im::Table("grouped", 3, TABLE_FLAGS | ImGuiTableFlags_SizingFixedFit, [&]{
		ImGui::TableSetupColumn("Device / ports", ImGuiTableColumnFlags_WidthFixed |
		                                          ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed |
		                                 ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();

		for (const auto& group : getGroups(cpuInterface)) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			im::TreeNode(group.device.c_str(),
			             ImGuiTreeNodeFlags_DefaultOpen |
			             ImGuiTreeNodeFlags_SpanFullWidth, [&]{
				for (const auto& range : group.ranges) {
					ImGui::TableNextRow();
					if (ImGui::TableNextColumn()) { // ports
						bool conflict = range.overlap && warnOverlap;
						im::ScopedFont sf(manager.fontMono);
						im::StyleColor(conflict, ImGuiCol_Text,
						               getColor(imColor::YELLOW), [&]{
							ImGui::TextUnformatted(portsText(range));
						});
						if (conflict) simpleToolTip(OVERLAP_TOOLTIP);
					}
					if (ImGui::TableNextColumn()) { // direction
						ImGui::TextUnformatted(dirText(range));
					}
					if (ImGui::TableNextColumn()) { // value(s)
						drawValue(cpuInterface, range, time);
					}
				}
			});
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
		ImGui::Checkbox("Group by device", &groupByDevice);
		simpleToolTip("A device doesn't necessarily occupy a contiguous range of "
		              "ports. Group by device to see all ports of one device "
		              "together instead of sorted by port number.");

		auto& cpuInterface = motherBoard->getCPUInterface();
		auto time = motherBoard->getCurrentTime();
		if (groupByDevice) {
			drawGrouped(cpuInterface, warnOverlap, time);
		} else {
			drawFlat(cpuInterface, warnOverlap, time);
		}
	});
}

} // namespace openmsx
