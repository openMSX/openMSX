#include "ImGuiOpenFile.hh"
#include "ImGuiManager.hh"

#include "FilePool.hh"
#include "Reactor.hh"
#include "ReadDir.hh"

#include "FileOperations.hh"
#include "one_of.hh"

#include <imgui.h>
#include <ImGuiFileDialog.h>

namespace openmsx {

void ImGuiOpenFile::save(ImGuiTextBuffer& buf)
{
	for (const auto& [key, value] : lastPath) {
		buf.appendf("%s=%s\n", key.c_str(), value.c_str());
	}
	auto bookmarks = ImGuiFileDialog::Instance()->SerializeBookmarks();
	buf.appendf("bookmarks=%s\n", bookmarks.c_str());
}

void ImGuiOpenFile::loadLine(std::string_view name, zstring_view value)
{
	if (name == "bookmarks") {
		ImGuiFileDialog::Instance()->DeserializeBookmarks(std::string(value));
	} else {
		lastPath[std::string(name)] = value;
	}
}

[[nodiscard]] static bool dirOnlyHasReadme(zstring_view directory)
{
	ReadDir dir(directory);
	while (auto* d = dir.getEntry()) {
		auto name = std::string_view(d->d_name);
		if (name == one_of(std::string_view(".."), std::string_view("."))) continue;
		if (std::string_view(d->d_name) != "README") return false;
	}
	return true;
}

[[nodiscad]] static std::string makeUniqueName(std::string_view proposedName, std::vector<std::string>& existingNames)
{
	std::string name{proposedName};
	int count = 1;
	while (contains(existingNames, name)) {
		name = strCat(proposedName, " (", ++count, ')');
	}
	existingNames.push_back(name);
	return name;
}

void ImGuiOpenFile::setBookmarks()
{
	auto& dialog = *ImGuiFileDialog::Instance();

	auto& filePool = manager.getReactor().getFilePool();
	std::vector<std::string> existingNames;
	for (const auto& dir : filePool.getDirectories()) {
		//using enum FileType; // c++20, but needs gcc-11
		//if ((dir.types & (ROM | DISK | TAPE)) == NONE) continue;
		if ((dir.types & (FileType::ROM | FileType::DISK | FileType::TAPE)) == FileType::NONE) continue;

		auto path = FileOperations::getNativePath(std::string(dir.path));
		if (!FileOperations::isDirectory(path)) continue;
		if (dirOnlyHasReadme(path)) continue;
		auto name = makeUniqueName(FileOperations::getFilename(dir.path), existingNames);
		dialog.RemoveBookmark(name);
		dialog.AddBookmark(name, path);
	}
}

std::string ImGuiOpenFile::getStartPath(zstring_view lastLocationHint)
{
	if (!lastLocationHint.empty()) {
		if (auto stat = FileOperations::getStat(lastLocationHint)) {
			if (FileOperations::isDirectory(*stat)) {
				return std::string(lastLocationHint);
			} else if (FileOperations::isRegularFile(*stat)) {
				return std::string(FileOperations::getDirName(lastLocationHint));
			}
		}
	}
	auto [it, inserted] = lastPath.try_emplace(lastFileDialog, ".");
	return it->second;
}

void ImGuiOpenFile::selectFile(const std::string& title, std::string filters,
                               std::function<void(const std::string&)> callback,
                               zstring_view lastLocationHint)
{
	setBookmarks();

	filters += ",All files (*){.*}";
	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
		ImGuiFileDialogFlags_CaseInsensitiveExtention |
		ImGuiFileDialogFlags_Modal |
		ImGuiFileDialogFlags_DisableCreateDirectoryButton;
	lastFileDialog = title;

	auto startPath = getStartPath(lastLocationHint);
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, filters.c_str(), startPath, "", 1, nullptr, flags);
	openFileCallback = callback;
}

void ImGuiOpenFile::selectNewFile(const std::string& title, std::string filters,
                                  std::function<void(const std::string&)> callback,
                                  zstring_view lastLocationHint)
{
	setBookmarks();

	filters += ",All files (*){.*}";
	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
		ImGuiFileDialogFlags_CaseInsensitiveExtention |
		ImGuiFileDialogFlags_Modal |
		ImGuiFileDialogFlags_DisableCreateDirectoryButton |
		ImGuiFileDialogFlags_ConfirmOverwrite;
	lastFileDialog = title;

	auto startPath = getStartPath(lastLocationHint);
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, filters.c_str(), startPath, "", 1, nullptr, flags);
	openFileCallback = callback;
}

void ImGuiOpenFile::selectDirectory(const std::string& title,
                                    std::function<void(const std::string&)> callback,
                                    zstring_view lastLocationHint)
{
	setBookmarks();

	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
		ImGuiFileDialogFlags_CaseInsensitiveExtention |
		ImGuiFileDialogFlags_Modal;
	lastFileDialog = title;

	auto startPath = getStartPath(lastLocationHint);
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, nullptr, startPath, "", 1, nullptr, flags);
	openFileCallback = callback;
}

void ImGuiOpenFile::paint(MSXMotherBoard* /*motherBoard*/)
{
	// (Modal) file dialog
	auto* fileDialog = ImGuiFileDialog::Instance();
	if (fileDialog->Display("FileDialog", ImGuiWindowFlags_NoCollapse, ImVec2(480.0f, 360.0f))) {
		if (fileDialog->IsOk() && openFileCallback) {
			lastPath[lastFileDialog] = fileDialog->GetCurrentPath();
			std::string filePathName = FileOperations::getConventionalPath( // Windows: replace backslash with slash
				fileDialog->GetFilePathName());
			openFileCallback(filePathName);
			openFileCallback = {};
		}
		fileDialog->Close();
	}
}

} // namespace openmsx
