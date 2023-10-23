#include "ImGuiDiskManipulator.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Date.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"
#include "unreachable.hh"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cassert>

namespace openmsx {

ImGuiDiskManipulator::ImGuiDiskManipulator(ImGuiManager& manager_)
	: manager(manager_)
{
}

void ImGuiDiskManipulator::refreshHost()
{
	if (hostDir.empty() || !FileOperations::isDirectory(hostDir)) {
		hostDir = FileOperations::getCurrentWorkingDirectory();
		editHostDir = hostDir;
	}

	hostFileCache.clear();
	auto fileAction = [&](const std::string& /*fullPath*/, std::string_view filename, const FileOperations::Stat& st) {
		FileInfo info;
		info.filename = std::string{filename};
		info.size = st.st_size;
		info.modified = st.st_mtime;
		info.isDirectory = false;
		hostFileCache.push_back(std::move(info));
	};
	auto directoryAction = [&](const std::string& /*fullPath*/, std::string_view filename, const FileOperations::Stat& st) {
		FileInfo info;
		info.filename = std::string{filename};
		info.size = size_t(-1); // not shown, but used for sorting
		info.modified = st.st_mtime;
		info.isDirectory = true;
		hostFileCache.push_back(std::move(info));
	};
	foreach_file_and_directory(hostDir, fileAction, directoryAction);

	hostLastClick = -1;
	hostNeedRefresh = false;
	hostForceSort = true;
}

void ImGuiDiskManipulator::checkSort(std::vector<FileInfo>& files, bool& forceSort)
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!forceSort && !sortSpecs->SpecsDirty) return;

	forceSort = false;
	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // filename
		sortUpDown_String(files, sortSpecs, &FileInfo::filename);
		break;
	case 1: // size
		sortUpDown_T(files, sortSpecs, &FileInfo::size);
		break;
	case 2: // modified
		sortUpDown_T(files, sortSpecs, &FileInfo::modified);
		break;
	case 3: // attrib
		sortUpDown_String(files, sortSpecs, &FileInfo::attrib);
		break;
	default:
		UNREACHABLE;
	}
}

std::string_view ImGuiDiskManipulator::drawTable(std::vector<FileInfo>& files, int& lastClickIdx, bool& forceSort, bool drawAttrib)
{
	auto clearSelection = [&]{
		for (auto& file : files) file.isSelected = false;
	};
	auto selectRange = [&](size_t curr, size_t last) {
		assert(!files.empty());
		assert(curr < files.size());
		if (last >= files.size()) last = files.size() - 1;

		auto begin = std::min(curr, last);
		auto end   = std::max(curr, last);
		for (auto i = begin; i <= end; ++i) {
			files[i].isSelected = true;
		}
	};

	checkSort(files, forceSort);

	std::string_view result;
	ImGuiListClipper clipper; // only draw the actually visible lines
	clipper.Begin(narrow<int>(files.size()));
	while (clipper.Step()) {
		for (int i : xrange(clipper.DisplayStart, clipper.DisplayEnd)) {
			im::ID(i, [&]{
				auto& file = files[i];
				if (ImGui::TableNextColumn()) { // filename
					auto pos = ImGui::GetCursorPos();
					if (ImGui::Selectable("##row", file.isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick)) {
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
							result = file.filename;
						} else {
							bool ctrl     = ImGui::IsKeyDown(ImGuiKey_LeftCtrl ) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
							bool shiftKey = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
							bool shift = shiftKey && (lastClickIdx != -1);
							if (ctrl && shift) {
								selectRange(i, lastClickIdx);
							} else if (ctrl) {
								file.isSelected = !file.isSelected;
								lastClickIdx = i;
							} else if (shift) {
								clearSelection();
								selectRange(i, lastClickIdx);
							} else {
								clearSelection();
								file.isSelected = true;
								lastClickIdx = i;
							}
						}
					}
					ImGui::SetCursorPos(pos);

					ImGui::Text("%s %s",
						file.isDirectory ? ICON_IGFD_FOLDER_OPEN : ICON_IGFD_FILE,
						file.filename.c_str());
				}
				if (ImGui::TableNextColumn()) { // size
					if (!file.isDirectory) {
						ImGui::Text("%zu", file.size);
					}
				}
				if (ImGui::TableNextColumn()) { // modified
					ImGui::TextUnformatted(Date::toString(file.modified));
				}
				if (drawAttrib && ImGui::TableNextColumn()) { // attrib
					ImGui::TextUnformatted(file.attrib);
				}
			});
		}
	}
	return result;
}

void ImGuiDiskManipulator::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!show) return;

	if (hostNeedRefresh) {
		refreshHost();
	}

	bool createHostDir = false;

	// TODO inital window size
	im::Window("Disk Manipulator (mockup)", &show, [&]{
		const auto& style = ImGui::GetStyle();
		auto availableSize = ImGui::GetContentRegionAvail();
		auto tSize = ImGui::CalcTextSize(">>");
		auto bWidth = 2.0f * (style.ItemSpacing.x + style.FramePadding.x) + tSize.x;
		auto cWidth = (availableSize.x - bWidth) * 0.5f;

		int tableFlags = ImGuiTableFlags_RowBg |
		                 ImGuiTableFlags_BordersV |
		                 ImGuiTableFlags_BordersOuter |
		                 ImGuiTableFlags_ScrollY |
		                 ImGuiTableFlags_Resizable |
		                 ImGuiTableFlags_Reorderable |
		                 ImGuiTableFlags_Hideable |
		                 ImGuiTableFlags_Sortable |
		                 ImGuiTableFlags_ContextMenuInBody |
		                 ImGuiTableFlags_ScrollX;
		im::Child("##msx", {cWidth, 0.0f}, [&]{
			auto b2Width = 2.0f * style.ItemSpacing.x + 4.0f * style.FramePadding.x +
			               ImGui::CalcTextSize(ICON_IGFD_ADD ICON_IGFD_FOLDER_OPEN).x;
			ImGui::SetNextItemWidth(-b2Width);
			im::Combo("##drive", "virtual drive", [&]{
			});
			ImGui::SameLine();
			ImGui::Button(ICON_IGFD_ADD"##NewDiskImage");
			ImGui::SameLine();
			ImGui::Button(ICON_IGFD_FOLDER_OPEN"##BrowseDiskImage");

			ImGui::Button(ICON_IGFD_CHEVRON_UP"##msxDirUp");
			ImGui::SameLine();
			ImGui::Button(ICON_IGFD_REFRESH"##msxRefresh");
			ImGui::SameLine();
			ImGui::Button(ICON_IGFD_ADD"##msxNewDir");
			ImGui::SameLine();
			std::string msxPath = "/";
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputText("##msxPath", &msxPath);

			im::Table("##msxFiles", 4, tableFlags, {-FLT_MIN, -ImGui::GetFrameHeightWithSpacing()}, [&]{
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoReorder);
				ImGui::TableSetupColumn("Size");
				ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_DefaultHide);
				ImGui::TableSetupColumn("Attrib", ImGuiTableColumnFlags_DefaultHide);
				ImGui::TableHeadersRow();
				drawTable(msxFileCache, msxLastClick, msxForceSort, true);
			});

			ImGui::Button("Export to new disk image");
		});

		ImGui::SameLine();
		auto pos1 = ImGui::GetCursorPos();
		auto b2Height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight();
		auto byPos = (availableSize.y - b2Height) * 0.5f;
		im::Group([&]{
			ImGui::Dummy({0.0f, byPos});
			ImGui::Button("<<");
			ImGui::Button(">>");
		});
		ImGui::SameLine();
		ImGui::SetCursorPosY(pos1.y);

		im::Child("##host", {cWidth, 0.0f}, [&]{
			if (ImGui::Button(ICON_IGFD_CHEVRON_UP"##hostDirUp")) hostParentDirectory();
			simpleToolTip("Go up one level");
			ImGui::SameLine();
			if (ImGui::Button(ICON_IGFD_REFRESH"##hostRefresh")) hostRefresh();
			simpleToolTip("Refresh directory listing");
			ImGui::SameLine();
			if (ImGui::Button(ICON_IGFD_ADD"##hostNewDir")) createHostDir = true;
			simpleToolTip("New directory");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			im::StyleColor(!FileOperations::isDirectory(editHostDir), ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f}, [&]{
				if (ImGui::InputText("##hostPath", &editHostDir)) {
					if (FileOperations::isDirectory(editHostDir)) {
						hostDir = editHostDir;
						hostRefresh();
					}
				}
			});

			im::Table("##hostFiles", 3, tableFlags, {-FLT_MIN, -FLT_MIN}, [&]{
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoReorder);
				ImGui::TableSetupColumn("Size");
				ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_DefaultHide);
				ImGui::TableHeadersRow();
				auto doubleClicked = drawTable(hostFileCache, hostLastClick, hostForceSort, false);
				if (!doubleClicked.empty()) {
					hostDir = FileOperations::join(hostDir, doubleClicked);
					hostRefresh();
				}
			});
		});
	});

	const char* const newHostDirTitle = "Create new host directory";
	if (createHostDir) {
		editNewDir.clear();
		ImGui::OpenPopup(newHostDirTitle);
	}
	im::PopupModal(newHostDirTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		ImGui::TextUnformatted("Create new directory inside:");
		ImGui::TextUnformatted(hostDir);
		bool close = false;
		bool ok = ImGui::InputText("##hostPath", &editNewDir, ImGuiInputTextFlags_EnterReturnsTrue);
		ok |= ImGui::Button("Ok");
		if (ok) {
			FileOperations::mkdirp(FileOperations::join(hostDir, editNewDir));
			hostRefresh();
			close = true;
		}
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) ImGui::CloseCurrentPopup();
	});
}

void ImGuiDiskManipulator::hostParentDirectory()
{
	if (hostDir.ends_with('/')) hostDir.pop_back();
	hostDir = FileOperations::getDirName(hostDir);
	if (hostDir.empty()) hostDir.push_back('/');
	hostRefresh();
}

void ImGuiDiskManipulator::hostRefresh()
{
	editHostDir = hostDir;
	hostNeedRefresh = true;
}

} // namespace openmsx
