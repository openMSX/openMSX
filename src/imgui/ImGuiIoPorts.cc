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

void ImGuiIoPorts::sortRows(std::vector<Row>& rows)
{
	// Note: unlike the other views in openMSX, 'rows' is rebuilt from scratch
	// every frame. So (re)apply the sort unconditionally instead of only when
	// 'SpecsDirty' is set.
	const auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs || (sortSpecs->SpecsCount == 0)) return;
	assert(sortSpecs->Specs);

	// 'rows' is generated in ascending port order, and all sorts below are
	// stable. So e.g. sorting on device keeps each device's ports ordered.
	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // ports
		sortUpDown_T(rows, sortSpecs, &Row::begin);
		break;
	case 1: // direction
		sortUpDown_String(rows, sortSpecs, [](const Row& row) { return dirText(row); });
		break;
	case 2: // device
		sortUpDown_String(rows, sortSpecs, &Row::device);
		break;
	default:
		UNREACHABLE;
	}
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

void ImGuiIoPorts::drawTable(MSXCPUInterface& cpuInterface, bool warnOverlap, EmuTime time)
{
	// 'Ports' and 'Dir' have a fixed width, so their width must fit the header
	// including the sort arrow that appears next to it once you sort on them.
	// Otherwise those (narrow) headers would stay clipped to "...".
	const auto& style = ImGui::GetStyle();
	auto arrowWidth = ImGui::GetFontSize() + style.ItemInnerSpacing.x;
	auto monoWidth = [&](std::string_view s) {
		im::ScopedFont sf(manager.fontMono);
		return ImGui::CalcTextSize(s).x;
	};
	auto sortableWidth = [&](std::string_view header, float contentWidth) {
		return std::max(ImGui::CalcTextSize(header).x + arrowWidth, contentWidth) +
		       2.0f * style.CellPadding.x;
	};

	im::Table("ports", 4, TABLE_FLAGS | ImGuiTableFlags_Sortable, [&]{
		// 'Value' is hidden by default: the hex editor on the 'ioports'
		// debuggable already shows these values. Un-hide it via the
		// right-click context menu on the table header.
		// It's also not sortable: the values change while the emulation
		// runs, so sorting on them would reshuffle the rows every frame.
		//
		// 'Ports' and 'Dir' hold at most 5 resp. 3 characters, so there is
		// nothing to gain by resizing them: keep them at their content width
		// and let all remaining space go to 'Device'.
		ImGui::TableSetupColumn("Ports", ImGuiTableColumnFlags_WidthFixed |
		                                 ImGuiTableColumnFlags_NoResize |
		                                 ImGuiTableColumnFlags_DefaultSort,
		                        sortableWidth("Ports", monoWidth("FF-FF")));
		ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed |
		                               ImGuiTableColumnFlags_NoResize,
		                        sortableWidth("Dir", ImGui::CalcTextSize("I/O"sv).x));
		ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed |
		                                 ImGuiTableColumnFlags_DefaultHide |
		                                 ImGuiTableColumnFlags_NoSort);
		ImGui::TableHeadersRow();

		auto rows = getRows(cpuInterface);
		sortRows(rows);
		for (const auto& row : rows) {
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
		drawTable(motherBoard->getCPUInterface(), warnOverlap,
		          motherBoard->getCurrentTime());
	});
}

} // namespace openmsx
