#include "ImGuiOpenFile.hh"
#include "ImGuiManager.hh"

#include "Reactor.hh"
#include "FilePool.hh"

#include "FileOperations.hh"

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

void ImGuiOpenFile::setBookmarks()
{
	auto& dialog = *ImGuiFileDialog::Instance();

	auto& filePool = manager.getReactor().getFilePool();
	int count = 0;
	for (const auto& dir : filePool.getDirectories()) {
		using enum FileType;
		if ((dir.types & (ROM | DISK | TAPE)) == NONE) continue;

		auto name = strCat("file pool ", ++count);
		dialog.RemoveBookmark(name);
		dialog.AddBookmark(name, FileOperations::getNativePath(std::string(dir.path)));
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
	//flags |= ImGuiFileDialogFlags_ConfirmOverwrite |
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
