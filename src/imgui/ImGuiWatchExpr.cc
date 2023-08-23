#include "ImGuiWatchExpr.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CommandException.hh"

#include "narrow.hh"
#include "xrange.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

#include <cassert>

namespace openmsx {

using namespace std::literals;

void ImGuiWatchExpr::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& watch : watches) {
		auto out = makeTclList(watch.description, watch.expression, watch.format);
		buf.appendf("watch=%s\n", out.getString().c_str());
	}
}

void ImGuiWatchExpr::loadStart()
{
	watches.clear();
}

void ImGuiWatchExpr::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "watch"sv) {
		TclObject in(value);
		if (in.size() == 3) {
			WatchExpr result;
			result.description = in.getListIndexUnchecked(0).getString();
			result.expression  = in.getListIndexUnchecked(1);
			result.format      = in.getListIndexUnchecked(2);
			watches.push_back(std::move(result));
		}
	}
}

void ImGuiWatchExpr::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!show) return;

	ImGui::SetNextWindowSize(gl::vec2{35, 15} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Watch expression", &show, [&]{
		im::Child("child", {-64, 0}, [&] {
			int flags = ImGuiTableFlags_Resizable
				| ImGuiTableFlags_Reorderable
				| ImGuiTableFlags_Hideable
				| ImGuiTableFlags_Sortable
				| ImGuiTableFlags_RowBg
				| ImGuiTableFlags_BordersV
				| ImGuiTableFlags_BordersOuter
				| ImGuiTableFlags_SizingStretchProp
				| ImGuiTableFlags_SortTristate;
			im::Table("table", 4, flags, [&]{
				ImGui::TableSetupColumn("description");
				ImGui::TableSetupColumn("expression");
				ImGui::TableSetupColumn("format");
				ImGui::TableSetupColumn("result", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoHide);
				ImGui::TableHeadersRow();
				checkSort();

				for (auto row : xrange(narrow<int>(watches.size()))) {
					im::ID(row, [&]{
						drawRow(row);
					});
				}
			});
		});
		ImGui::SameLine();
		im::Group([&] {
			if (ImGui::Button("Add")) {
				selectedRow = narrow<int>(watches.size()); // select created row
				watches.emplace_back();
			}
			im::Disabled(selectedRow < 0 || selectedRow >= narrow<int>(watches.size()), [&]{
				if (ImGui::Button("Remove")) {
					watches.erase(watches.begin() + selectedRow);
					if (selectedRow == narrow<int>(watches.size())) {
						selectedRow = -1;
					}
				}
			});
			ImGui::Dummy({0, 20});
			if (ImGui::SmallButton("Examples")) {
				watches.emplace_back(
					"peek at fixed address",
					TclObject("[peek 0xfcaf]"),
					TclObject("screen=%d"));
				watches.emplace_back(
					"VDP command executing",
					TclObject("[debug read \"VDP status regs\" 2] & 1"),
					TclObject());
				watches.emplace_back(
					"PSG enable-channel status",
					TclObject("[debug read \"PSG regs\" 7]"),
					TclObject("%08b"));
				watches.emplace_back(
					"The following 2 require an appropriate symbol file",
					TclObject(""),
					TclObject(""));
				watches.emplace_back(
					"value of 'myLabel'",
					TclObject("$sym(myLabel)"),
					TclObject("0x%04x"));
				watches.emplace_back(
					"peek at symbolic address",
					TclObject("[peek16 $sym(numItems)]"),
					TclObject("%d items"));
			}
			ImGui::Dummy({0, 0}); // want help marker on the next line
			HelpMarker(R"(The given (Tcl) expressions are continuously evaluated and displayed in a nicely formatted way.
Press the 'Examples' button to see some examples.

There are 4 columns:
* description: optional description for the expression
* expression: the actual Tcl expression, see below
* format: optional format specifier, see below
* result: this shows the result of evaluating 'expression'

Add a new entry via the 'Add' button, then fill in the appropriate fields.
To remove an entry, first select a row (e.g. click on the corresponding 'result' cell), then press the 'Remove' button.

You can sort the table by clicking the column headers.
You can reorder the columns by dragging the column headers.
You can hide columns via the right-click context menu on a column header. For example, once configured, you may want to hide the 'expression' and 'format' columns.

The expression can be any Tcl expression, some examples:
* [peek 0x1234]: monitor a byte at a specific address
* [reg SP] < 0xe000: check that stack hasn't grown too large
                     (below address 0xe000)
If you have debug-symbols loaded (via the 'Symbol manager'), you can use them like:
* [peek16 $sym(mySymbol)]: monitor 16-bit value at 'mySymbol'

In the format column you can optionally enter a format-specifier (for the  Tcl 'format' command). Some examples:
* 0x%04x: 4-digit hex with leading zeros and '0x' prefix
* %08b: 8-bit binary value
* %d items: decimal value followed by the string " items"
)");
		});
	});
}

static void tooWideToolTip(float available, zstring_view str)
{
	if (str.empty()) return;
	auto width = ImGui::CalcTextSize(str.c_str()).x;
	width += 2.0f * ImGui::GetStyle().FramePadding.x;
	if (width >= available) {
		simpleToolTip(str);
	}
}

void ImGuiWatchExpr::drawRow(int row)
{
	auto& interp = manager.getInterpreter();
	auto& watch = watches[row];

	// evaluate 'expression'
	TclObject result;
	std::string exprError;
	try {
		if (!watch.expression.getString().empty()) {
			result = watch.expression.eval(interp);
		}
	} catch (CommandException& e) {
		exprError = e.getMessage();
	}
	bool validExpr = exprError.empty();

	// format the result
	TclObject frmtResult = result; // also fallback for error in format
	std::string frmtError;
	if (!watch.format.getString().empty()) {
		auto frmtCmd = makeTclList("format", watch.format, validExpr ? result : TclObject("0"));
		try {
			frmtResult = frmtCmd.executeCommand(interp);
		} catch (CommandException& e) {
			frmtError = e.getMessage();
		}
	}
	bool validFrmt = frmtError.empty();

	if (ImGui::TableNextColumn()) { // description
		auto pos = ImGui::GetCursorPos();
		const auto& style = ImGui::GetStyle();
		float rowHeight = 2.0f * style.FramePadding.y;
		rowHeight += ImGui::CalcTextSize((validExpr ? frmtResult.getString() : exprError).c_str()).y;
		if (ImGui::Selectable("##selection", selectedRow == row,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
				{0.0f, rowHeight})) {
			selectedRow = row;
		}
		ImGui::SetCursorPos(pos);

		auto avail = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##desc", &watch.description);
		tooWideToolTip(avail, watch.description);
	}
	if (ImGui::TableNextColumn()) { // expression
		im::StyleColor(ImGuiCol_Text, validExpr ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
							: ImVec4(1.0f, 0.5f, 0.5f, 1.0f), [&]{
			auto avail = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(-FLT_MIN);
			auto str = std::string(watch.expression.getString());
			if (ImGui::InputText("##expr", &str)) {
				watch.expression = str;
			}
			tooWideToolTip(avail, str);
		});
	}
	if (ImGui::TableNextColumn()) { // format
		im::StyleColor(ImGuiCol_Text, validFrmt ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
							: ImVec4(1.0f, 0.5f, 0.5f, 1.0f), [&]{
			auto avail = ImGui::GetContentRegionAvail().x;
			ImGui::SetNextItemWidth(-FLT_MIN);
			auto str = std::string(watch.format.getString());
			if (ImGui::InputText("##format", &str)) {
				watch.format = str;
			}
			if (validFrmt) {
				tooWideToolTip(avail, str);
			} else {
				simpleToolTip(frmtError);
			}
		});
	}
	if (ImGui::TableNextColumn()) { // result
		auto avail = ImGui::GetContentRegionAvail().x;
		if (validExpr) {
			const auto& str = frmtResult.getString();
			ImGui::TextUnformatted(str);
			tooWideToolTip(avail, str);
		} else {
			im::StyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f), [&]{
				ImGui::TextUnformatted(exprError);
				tooWideToolTip(avail, exprError);
			});
		}
	}
}

void ImGuiWatchExpr::checkSort()
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	if (sortSpecs->SpecsCount == 0) return;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	auto sortUpDown = [&](auto proj) {
		if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending) {
			ranges::stable_sort(watches, std::greater<>{}, proj);
		} else {
			ranges::stable_sort(watches, std::less<>{}, proj);
		}
	};
	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // description
		sortUpDown(&WatchExpr::description);
		break;
	case 1: // expression
		sortUpDown([](const auto& item) { return item.expression.getString(); });
		break;
	case 2: // format
		sortUpDown([](const auto& item) { return item.format.getString(); });
		break;
	default:
		UNREACHABLE;
	}
}

} // namespace openmsx
