#include "ImGuiSymbols.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CliComm.hh"
#include "File.hh"
#include "Interpreter.hh"
#include "MSXException.hh"
#include "Reactor.hh"

#include "enumerate.hh"
#include "ranges.hh"
#include "StringOp.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <imgui.h>

#include <cassert>

namespace openmsx {

ImGuiSymbols::ImGuiSymbols(ImGuiManager& manager_)
	: manager(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
	symbolManager.setObserver(this);
}

ImGuiSymbols::~ImGuiSymbols()
{
	symbolManager.setObserver(nullptr);
}

void ImGuiSymbols::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& file : symbolManager.getFiles()) {
		buf.appendf("symbolfile=%s\n", file.filename.c_str());
		// TODO type
	}
	for (const auto& [file, error, type] : fileError) {
		buf.appendf("symbolfile=%s\n", file.c_str());
		// TODO type
	}
}

void ImGuiSymbols::loadStart()
{
	symbolManager.removeAllFiles();
}

void ImGuiSymbols::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "symbolfile") {
		std::string filename{value};
		try {
			auto type = SymbolFile::Type::AUTO_DETECT; // TODO
			symbolManager.reloadFile(filename, true, type);
		} catch (MSXException& e) {
			fileError.emplace_back(filename, e.getMessage());
		}
	}
}

void ImGuiSymbols::loadEnd()
{
	// TODO only load here?
}

static void checkSort(const SymbolManager& manager, std::vector<SymbolRef>& symbols)
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // name
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.name(manager); });
		break;
	case 1: // value
		sortUpDown_T(symbols, sortSpecs, [&](const auto& sym) { return sym.value(manager); });
		break;
	case 2: // file
		sortUpDown_String(symbols, sortSpecs, [&](const auto& sym) { return sym.file(manager); });
		break;
	default:
		UNREACHABLE;
	}
}
template<bool FILTER_FILE>
static void drawTable(SymbolManager& manager, std::vector<SymbolRef>& symbols, const std::string& file = {})
{
	assert(FILTER_FILE == !file.empty());

	int flags = ImGuiTableFlags_RowBg |
	            ImGuiTableFlags_BordersV |
	            ImGuiTableFlags_BordersOuter |
	            ImGuiTableFlags_ContextMenuInBody |
	            ImGuiTableFlags_Resizable |
	            ImGuiTableFlags_Reorderable |
	            ImGuiTableFlags_Sortable |
	            ImGuiTableFlags_SizingStretchProp |
	            (FILTER_FILE ? ImGuiTableFlags_ScrollY : 0);
	im::Table(file.c_str(), FILTER_FILE ? 2 : 3, flags, {0, 100}, [&]{
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("name");
		ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed);
		if (!FILTER_FILE) {
			ImGui::TableSetupColumn("file");
		}
		ImGui::TableHeadersRow();
		checkSort(manager, symbols);

		for (const auto& sym : symbols) {
			if (FILTER_FILE && (sym.file(manager) != file)) continue;

			if (ImGui::TableNextColumn()) { // name
				ImGui::TextUnformatted(sym.name(manager));
			}
			if (ImGui::TableNextColumn()) { // value
				ImGui::StrCat(hex_string<4>(sym.value(manager)));
			}
			if (!FILTER_FILE && ImGui::TableNextColumn()) { // file
				ImGui::TextUnformatted(sym.file(manager));
			}
		}
	});
}

void ImGuiSymbols::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!show) return;

	ImGui::SetNextWindowSize(gl::vec2{24, 18} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Symbol Manager", &show, [&]{
		if (ImGui::Button("Load symbol file...")) {
			manager.openFile.selectFile(
				"Select symbol file",
				SymbolManager::getFileFilters(),
				[this](const std::string& filename) {
					auto type = SymbolManager::getTypeForFilter(ImGuiOpenFile::getLastFilter());
					if (!symbolManager.reloadFile(filename, false, type)) {
						// TODO warning empty file
					}
				});
		}

		im::TreeNode("Symbols per file", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto drawFile = [&](const FileInfo& info) {
				im::StyleColor(!info.error.empty(), ImGuiCol_Text, {1.0f, 0.5f, 0.5f, 1.0f}, [&]{
					auto title = strCat("File: ", info.filename);
					im::TreeNode(title.c_str(), [&]{
						if (!info.error.empty()) {
							ImGui::TextUnformatted(info.error);
						}
						im::StyleColor(ImGuiCol_Text, {1.0f, 1.0f, 1.0f, 1.0f}, [&]{
							if (ImGui::Button("Reload")) {
								symbolManager.reloadFile(info.filename, false, info.type);
							}
							ImGui::SameLine();
							if (ImGui::Button("Remove")) {
								symbolManager.removeFile(info.filename);
							}
							drawTable<true>(symbolManager, symbols, info.filename);
						});
					});
				});
			};

			// make copy because cache may get dropped
			auto infos = to_vector(view::transform(symbolManager.getFiles(), [](const auto& file) {
				return FileInfo{file.filename, std::string{}, file.type};
			}));
			for (const auto& info : infos) {
				drawFile(info);
			}
			for (const auto& info : fileError) {
				drawFile(info);
			}

		});
		im::TreeNode("All symbols", [&]{
			if (ImGui::Button("Reload all")) {
				symbolManager.reloadAll(false);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove all")) {
				symbolManager.removeAllFiles();
			}
			drawTable<false>(symbolManager, symbols);
		});
	});
}

void ImGuiSymbols::notifySymbolsChanged()
{
	symbols.clear();
	for (const auto& [fileIdx, file] : enumerate(symbolManager.getFiles())) {
		for (auto symbolIdx : xrange(file.symbols.size())) {
			symbols.emplace_back(narrow<unsigned>(fileIdx),
			                     narrow<unsigned>(symbolIdx));
		}
	}

	manager.breakPoints.refreshSymbols();
	manager.watchExpr.refreshSymbols();
}

} // namespace openmsx
