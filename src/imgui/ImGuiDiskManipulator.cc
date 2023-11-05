#include "ImGuiDiskManipulator.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "DiskChanger.hh"
#include "DiskImageUtils.hh"
#include "DiskManipulator.hh"
#include "DiskName.hh"
#include "FileOperations.hh"
#include "HD.hh"
#include "MSXtar.hh"

#include "Date.hh"
#include "foreach_file.hh"
#include "hash_set.hh"
#include "one_of.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "view.hh"
#include "xxhash.hh"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cassert>

namespace openmsx {

ImGuiDiskManipulator::ImGuiDiskManipulator(ImGuiManager& manager_)
	: manager(manager_)
{
}

DiskContainer* ImGuiDiskManipulator::getDrive()
{
	auto& diskManipulator = manager.getReactor().getDiskManipulator();
	return diskManipulator.getDrive(selectedDrive);
}

std::optional<DiskManipulator::DriveAndPartition> ImGuiDiskManipulator::getDriveAndDisk()
{
	auto& diskManipulator = manager.getReactor().getDiskManipulator();
	return diskManipulator.getDriveAndDisk(selectedDrive);
}

std::optional<ImGuiDiskManipulator::DrivePartitionTar> ImGuiDiskManipulator::getMsxStuff()
{
	auto dd = getDriveAndDisk();
	if (!dd) return {};
	auto& [drive, disk] = *dd;
	try {
		auto tar = std::make_unique<MSXtar>(*disk, manager.getReactor().getMsxChar2Unicode());
		return DrivePartitionTar{drive, std::move(disk), std::move(tar)};
	} catch (MSXException&) {
		// e.g. triggers when trying to parse a partition table as a FAT-disk
		return {};
	}
}

bool ImGuiDiskManipulator::isValidMsxDirectory(DrivePartitionTar& stuff, const std::string& dir)
{
	assert(dir.starts_with('/'));
	try {
		stuff.tar->chdir(dir);
		return true;
	} catch (MSXException&) {
		return false;
	}
}

std::vector<ImGuiDiskManipulator::FileInfo> ImGuiDiskManipulator::dirMSX(DrivePartitionTar& stuff)
{
	std::vector<FileInfo> result;
	// most likely nothing changed, and then we have the same number of entries
	result.reserve(msxFileCache.size());

	try {
		stuff.tar->chdir(msxDir);
	} catch (MSXException&) {
		msxDir = "/";
		editMsxDir = "/";
		stuff.tar->chdir(msxDir);
	}

	auto dir = stuff.tar->dirRaw();
	auto num = dir.size();
	for (unsigned i = 0; i < num; ++i) {
		auto entry = dir.getListIndexUnchecked(i);
		FileInfo info;
		info.attrib = entry.getListIndexUnchecked(1).getOptionalInt().value_or(0);
		if (info.attrib & MSXDirEntry::Attrib::VOLUME) continue; // skip
		info.filename = std::string(entry.getListIndexUnchecked(0).getString());
		if (info.filename == one_of(".", "..")) continue; // skip
		info.modified = entry.getListIndexUnchecked(2).getOptionalInt().value_or(0);
		info.size = entry.getListIndexUnchecked(3).getOptionalInt().value_or(0);
		info.isDirectory = info.attrib & MSXDirEntry::Attrib::DIRECTORY;
		if (info.isDirectory) info.size = size_t(-1);
		result.push_back(std::move(info));
	}

	return result;
}

void ImGuiDiskManipulator::refreshMsx(DrivePartitionTar& stuff)
{
	auto newFiles = dirMSX(stuff);
	if (newFiles.size() != msxFileCache.size()) msxLastClick = -1;

	// copy 'isSelected' from 'msxFileCache' to 'newFiles'
	hash_set<std::string_view, std::identity, XXHasher> selected;
	for (const auto& oldFile : msxFileCache) {
		if (oldFile.isSelected) selected.insert(oldFile.filename);
	}
	if (!selected.empty()) {
		for (auto& newFile : newFiles) {
			if (selected.contains(newFile.filename)) newFile.isSelected = true;
		}
	}

	std::swap(newFiles, msxFileCache);
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
		sortUpDown_T(files, sortSpecs, &FileInfo::attrib);
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
					ImGui::TextUnformatted(DiskImageUtils::formatAttrib(file.attrib));
				}
			});
		}
	}
	return result;
}

[[nodiscard]] static std::string driveDisplayName(std::string_view drive)
{
	if (drive == "virtual_drive") return "Virtual drive";
	std::string result;
	if (drive.size() >= 5 && drive.starts_with("disk")) {
		result = strCat("Disk drive ", char('A' + drive[4] - 'a'));
		drive.remove_prefix(5);
	} else if (drive.size() >= 3 && drive.starts_with("hd")) {
		result = strCat("Hard disk ", char('A' + drive[2] - 'a'));
		drive.remove_prefix(3);
	} else {
		return ""; // not supported
	}
	if (drive.empty()) return result; // Ok, no partition specified
	auto num = StringOp::stringToBase<10, unsigned>(drive);
	if (!num) return ""; // invalid format
	strAppend(result, ", partition ", *num);
	return result;
}

std::string ImGuiDiskManipulator::getDiskImageName()
{
	auto dd = getDriveAndDisk();
	if (!dd) return "";
	auto& [drive, disk] = *dd;
	if (auto* dr = dynamic_cast<DiskChanger*>(drive)) {
		return dr->getDiskName().getResolved();
	} else if (auto* hd = dynamic_cast<HD*>(drive)) {
		return hd->getImageName().getResolved();
	} else {
		return "";
	}
}

void ImGuiDiskManipulator::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!show) return;

	auto stuff = getMsxStuff();
	if (stuff) {
		refreshMsx(*stuff);
	} else {
		msxFileCache.clear();
	}
	if (hostNeedRefresh) {
		refreshHost();
	}

	bool createMsxDir = false;
	bool createHostDir = false;
	bool createDiskImage = false;

	const auto& style = ImGui::GetStyle();
	ImGui::SetNextWindowSize(gl::vec2{70, 45} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Disk Manipulator", &show, [&]{
		auto availableSize = ImGui::GetContentRegionAvail();
		auto tSize = ImGui::CalcTextSize(">>");
		auto bWidth = 2.0f * (style.ItemSpacing.x + style.FramePadding.x) + tSize.x;
		auto cWidth = (availableSize.x - bWidth) * 0.5f;

		bool writable = stuff && !stuff->disk->isWriteProtected();

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
			bool driveOk = false;
			im::Combo("##drive", driveDisplayName(selectedDrive).c_str(), [&]{
				auto& diskManipulator = manager.getReactor().getDiskManipulator();
				auto drives = diskManipulator.getDriveNamesForCurrentMachine();
				for (const auto& drive : drives) {
					if (selectedDrive == drive) driveOk = true;
					auto display = driveDisplayName(drive);
					if (display.empty()) continue;
					if (ImGui::Selectable(display.c_str())) {
						selectedDrive = drive;
						driveOk = true;
					}
				}
				if (!driveOk) {
					selectedDrive = "virtual_drive";
				}
			});
			simpleToolTip([&]{ return getDiskImageName(); });
			ImGui::SameLine();
			createDiskImage = ImGui::Button(ICON_IGFD_ADD"##NewDiskImage");
			simpleToolTip("Create new disk image");
			ImGui::SameLine();
			im::Disabled(selectedDrive.starts_with("hd"), [&]{
				if (ImGui::Button(ICON_IGFD_FOLDER_OPEN"##BrowseDiskImage")) insertMsxDisk();
				simpleToolTip("Insert disk image");
			});

			if (ImGui::Button(ICON_IGFD_CHEVRON_UP"##msxDirUp")) msxParentDirectory();
			simpleToolTip("Go to parent directory");
			ImGui::SameLine();
			im::Disabled(!writable, [&]{
				createMsxDir = ImGui::Button(ICON_IGFD_ADD"##msxNewDir");
				simpleToolTip("New MSX directory");
			});
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (stuff) {
				im::StyleColor(!isValidMsxDirectory(*stuff, editMsxDir), ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f}, [&]{
					if (ImGui::InputText("##msxPath", &editMsxDir)) {
						if (!editMsxDir.starts_with('/')) {
							editMsxDir = '/' + editMsxDir;
						}
						if (isValidMsxDirectory(*stuff, editMsxDir)) {
							msxDir = editMsxDir;
							msxRefresh();
						}
					}
				});
			} else {
				im::Disabled(true, []{
					std::string noDisk = "No (valid) disk inserted";
					ImGui::InputText("##msxPath", &noDisk);
				});
			}

			im::Table("##msxFiles", 4, tableFlags, {-FLT_MIN, -ImGui::GetFrameHeightWithSpacing()}, [&]{
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoReorder);
				ImGui::TableSetupColumn("Size");
				ImGui::TableSetupColumn("Modified");
				ImGui::TableSetupColumn("Attrib", ImGuiTableColumnFlags_DefaultHide);
				ImGui::TableHeadersRow();
				bool msxForceSort = true; // always sort
				auto doubleClicked = drawTable(msxFileCache, msxLastClick, msxForceSort, true);
				if (!doubleClicked.empty()) {
					msxDir = FileOperations::join(msxDir, doubleClicked);
					msxRefresh();
				}
			});

			im::Disabled(!stuff, [&]{
				if (ImGui::Button("Export to new disk image")) exportDiskImage();
				simpleToolTip("Save the content of the drive to a new disk image");
			});
			ImGui::SameLine();
			if (stuff) {
				auto [num, size] = stuff->tar->getFreeSpace();
				auto free = (size_t(num) * size) / 1024; // in kB
				auto status = strCat(free, "kB free");
				if (!writable) strAppend(status, ", read-only");
				ImGui::TextUnformatted(status);
			} else {
				ImGui::TextUnformatted("No (valid) disk inserted");
			}
		});

		ImGui::SameLine();
		auto pos1 = ImGui::GetCursorPos();
		auto b2Height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight();
		auto byPos = (availableSize.y - b2Height) * 0.5f;
		im::Group([&]{
			ImGui::Dummy({0.0f, byPos});
			im::Disabled(!writable || ranges::none_of(hostFileCache, &FileInfo::isSelected), [&]{
				if (ImGui::Button("<<")) transferHostToMsx(*stuff);
				simpleToolTip("Transfer files or directories from host to MSX");
			});
			im::Disabled(!stuff || ranges::none_of(msxFileCache, &FileInfo::isSelected), [&]{
				if (ImGui::Button(">>")) transferMsxToHost(*stuff);
				simpleToolTip("Transfer files or directories from MSX to host");
			});
		});
		ImGui::SameLine();
		ImGui::SetCursorPosY(pos1.y);

		im::Child("##host", {cWidth, 0.0f}, [&]{
			if (ImGui::Button(ICON_IGFD_CHEVRON_UP"##hostDirUp")) hostParentDirectory();
			simpleToolTip("Go to parent directory");
			ImGui::SameLine();
			if (ImGui::Button(ICON_IGFD_REFRESH"##hostRefresh")) hostRefresh();
			simpleToolTip("Refresh directory listing");
			ImGui::SameLine();
			createHostDir = ImGui::Button(ICON_IGFD_ADD"##hostNewDir");
			simpleToolTip("New host directory");
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
				ImGui::TableSetupColumn("Modified");
				ImGui::TableHeadersRow();
				auto doubleClicked = drawTable(hostFileCache, hostLastClick, hostForceSort, false);
				if (!doubleClicked.empty()) {
					hostDir = FileOperations::join(hostDir, doubleClicked);
					hostRefresh();
				}
			});
		});
	});

	const char* const newMsxDirTitle = "Create new MSX directory";
	if (createMsxDir) {
		editModal.clear();
		ImGui::OpenPopup(newMsxDirTitle);
	}
	im::PopupModal(newMsxDirTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		ImGui::TextUnformatted("Create new directory in:");
		ImGui::TextUnformatted(driveDisplayName(selectedDrive));
		ImGui::TextUnformatted(msxDir);
		bool close = false;
		bool ok = ImGui::InputText("##msxPath", &editModal, ImGuiInputTextFlags_EnterReturnsTrue);
		ok |= ImGui::Button("Ok");
		if (ok) {
			if (stuff) {
				try {
					stuff->tar->chdir(msxDir);
					stuff->tar->mkdir(editModal);
				} catch (MSXException& e) {
					manager.printError(
						"Couldn't create new MSX directory: ", e.getMessage());
				}
			}

			msxRefresh();
			close = true;
		}
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) ImGui::CloseCurrentPopup();
	});

	const char* const newHostDirTitle = "Create new host directory";
	if (createHostDir) {
		editModal.clear();
		ImGui::OpenPopup(newHostDirTitle);
	}
	im::PopupModal(newHostDirTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize, [&]{
		ImGui::TextUnformatted("Create new directory in:");
		ImGui::TextUnformatted(hostDir);
		bool close = false;
		bool ok = ImGui::InputText("##hostPath", &editModal, ImGuiInputTextFlags_EnterReturnsTrue);
		ok |= ImGui::Button("Ok");
		if (ok) {
			FileOperations::mkdirp(FileOperations::join(hostDir, editModal));
			hostRefresh();
			close = true;
		}
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) ImGui::CloseCurrentPopup();
	});

	const char* const newDiskImageTitle = "Create new disk image";
	if (createDiskImage) {
		auto current = getDiskImageName();
		auto cwd = current.empty() ? FileOperations::getCurrentWorkingDirectory()
		                           : std::string(FileOperations::getDirName(current));
		auto newName = FileOperations::getNextNumberedFileName(cwd, "new-image", ".dsk");
		editModal = FileOperations::join(cwd, newName);

		newDiskType = UNPARTITIONED;
		bootType = static_cast<int>(MSXBootSectorType::DOS2);
		unpartitionedSize = {720, PartitionSize::KB};
		partitionSizes.assign(3, {32, PartitionSize::MB});
		ImGui::OpenPopup(newDiskImageTitle);
	}
	ImGui::SetNextWindowSize(gl::vec2{35, 22} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	bool p_open = true; // initial value meaningless, but we need a ptr-to-bool to have a 'x' button
	im::PopupModal(newDiskImageTitle, &p_open, [&]{
		bool close = false;

		auto tSize = ImGui::CalcTextSize(ICON_IGFD_FOLDER_OPEN);
		auto bWidth = style.ItemSpacing.x + 2.0f * style.FramePadding.x + tSize.x;
		ImGui::SetNextItemWidth(-bWidth);
		ImGui::InputText("##newDiskPath", &editModal);
		simpleToolTip("Filename for new disk image");
		ImGui::SameLine();
		if (ImGui::Button(ICON_IGFD_FOLDER_OPEN"##BrowseNewImage")) {
			manager.openFile.selectNewFile(
				"Filename for new disk image",
				"Disk image (*.dsk){.dsk}", // only .dsk (not all other disk extensions)
				[&](const auto& fn) { editModal = fn; },
				getDiskImageName(),
				ImGuiOpenFile::Painter::DISKMANIPULATOR);
		}
		simpleToolTip("Select new file");
		if (manager.openFile.mustPaint(ImGuiOpenFile::Painter::DISKMANIPULATOR)) {
			manager.openFile.doPaint();
		}

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
		ImGui::Combo("type", &bootType, "DOS 1\0DOS 2\0Nextor\0", 3); // in sync with MSXBootSectorType enum
		ImGui::Separator();

		ImGui::RadioButton("No partitions (floppy disk)", &newDiskType, UNPARTITIONED);
		im::DisabledIndent(newDiskType != UNPARTITIONED, [&]{
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
			ImGui::InputScalar("##count", ImGuiDataType_U32, &unpartitionedSize.count);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5f);
			ImGui::Combo("##unit", &unpartitionedSize.unit, "kB\0MB\0", 2);
		});

		ImGui::RadioButton("Partitioned HD image", &newDiskType, PARTITIONED);
		im::DisabledIndent(newDiskType != PARTITIONED, [&]{
			float bottomHeight = 2.0f * style.ItemSpacing.y + 1.0f + 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();
			im::ListBox("##partitions", {14 * ImGui::GetFontSize(), -bottomHeight}, [&]{
				im::ID_for_range(partitionSizes.size(), [&](int i) {
					ImGui::AlignTextToFramePadding();
					ImGui::Text("%2d:", i + 1);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
					ImGui::InputScalar("##count", ImGuiDataType_U32, &partitionSizes[i].count);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5f);
					ImGui::Combo("##unit", &partitionSizes[i].unit, "kB\0MB\0", 2);
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				im::Disabled(partitionSizes.size() >= 31, [&]{
					if (ImGui::Button("Add")) {
						partitionSizes.push_back({32, PartitionSize::MB});
					}
				});
				im::Disabled(partitionSizes.size() <= 1, [&]{
					if (ImGui::Button("Remove")) {
						partitionSizes.pop_back();
					}
				});
			});
		});

		ImGui::Separator();
		im::Disabled(editModal.empty(), [&]{
			if (ImGui::Button("Ok")) {
				auto& diskManipulator = manager.getReactor().getDiskManipulator();
				auto sizes = [&]{
					if (newDiskType == UNPARTITIONED) {
						return std::vector<unsigned>(1, unpartitionedSize.asSectorCount());
					} else {
						return to_vector(view::transform(partitionSizes, &PartitionSize::asSectorCount));
					}
				}();
				try {
					diskManipulator.create(editModal, static_cast<MSXBootSectorType>(bootType), sizes);
					if (auto* drive = getDrive()) {
						drive->insertDisk(editModal); // might fail (return code), but ignore
						msxRefresh();
					}
				} catch (MSXException& e) {
					manager.printError(
						"Couldn't create disk image: ", e.getMessage());
				}
				close = true;
			}
		});
		ImGui::SameLine();
		close |= ImGui::Button("Cancel");
		if (close) ImGui::CloseCurrentPopup();
	});
}

void ImGuiDiskManipulator::insertMsxDisk()
{
	if (selectedDrive.starts_with("hd")) {
		// disallow changing HD image
		return;
	}
	manager.openFile.selectFile(
		strCat("Select disk image for ", driveDisplayName(selectedDrive)),
		ImGuiMedia::diskFilter(),
		[&](const auto& fn) {
			auto* drive = getDrive();
			if (!drive) return;
			drive->insertDisk(fn); // might fail (return code), but ignore
			msxRefresh();
		},
		getDiskImageName());
}

void ImGuiDiskManipulator::exportDiskImage()
{
	manager.openFile.selectNewFile(
		strCat("Export ", driveDisplayName(selectedDrive), " to new disk image"),
		"Disk image (*.dsk){.dsk}", // only .dsk (not all other disk extensions)
		[&](const auto& fn) {
			auto dd = getDriveAndDisk();
			if (!dd) return;
			auto& [drive, disk] = *dd;
			assert(disk);

			try {
				SectorBuffer buf;
				File file(fn, File::CREATE);
				for (auto i : xrange(disk->getNbSectors())) {
					disk->readSector(i, buf);
					file.write(buf.raw);
				}
			} catch (MSXException& e) {
				manager.printError(
					"Couldn't export disk image: ", e.getMessage());
			}
		},
		getDiskImageName());
}

void ImGuiDiskManipulator::msxParentDirectory()
{
	if (msxDir.ends_with('/')) msxDir.pop_back();
	msxDir = FileOperations::getDirName(msxDir);
	if (msxDir.empty()) msxDir.push_back('/');
	msxRefresh();
}

void ImGuiDiskManipulator::hostParentDirectory()
{
	if (hostDir.ends_with('/')) hostDir.pop_back();
	hostDir = FileOperations::getDirName(hostDir);
	if (hostDir.empty()) hostDir.push_back('/');
	hostRefresh();
}

void ImGuiDiskManipulator::msxRefresh()
{
	editMsxDir = msxDir;
	msxFileCache.clear();
}

void ImGuiDiskManipulator::hostRefresh()
{
	editHostDir = hostDir;
	hostNeedRefresh = true;
}

void ImGuiDiskManipulator::transferHostToMsx(DrivePartitionTar& stuff)
{
	try {
		stuff.tar->chdir(msxDir);
	} catch (MSXException& e) {
		msxRefresh();
		return;
	}
	for (const auto& item : hostFileCache) {
		if (!item.isSelected) continue;
		try {
			stuff.tar->addItem(FileOperations::join(hostDir, item.filename));
		} catch (MSXException& e) {
			manager.printError("Couldn't import ", item.filename, ": ", e.getMessage());
		}
	}
	msxRefresh();
}

void ImGuiDiskManipulator::transferMsxToHost(DrivePartitionTar& stuff)
{
	try {
		stuff.tar->chdir(msxDir);
	} catch (MSXException& e) {
		msxRefresh();
		return;
	}
	for (const auto& item : msxFileCache) {
		if (!item.isSelected) continue;
		try {
			stuff.tar->getItemFromDir(hostDir, item.filename);
		} catch (MSXException& e) {
			manager.printError("Couldn't extract ", item.filename, ": ", e.getMessage());
		}
	}
	hostRefresh();
}

} // namespace openmsx
