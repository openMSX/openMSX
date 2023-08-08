#include "ImGuiOpenFile.hh"
#include "ImGuiManager.hh"

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

void ImGuiOpenFile::selectFile(const std::string& title, std::string filters,
                            std::function<void(const std::string&)> callback)
{
	filters += ",All files (*){.*}";
	ImGuiFileDialogFlags flags =
		ImGuiFileDialogFlags_DontShowHiddenFiles |
		ImGuiFileDialogFlags_CaseInsensitiveExtention |
		ImGuiFileDialogFlags_Modal |
		ImGuiFileDialogFlags_DisableCreateDirectoryButton;
	//flags |= ImGuiFileDialogFlags_ConfirmOverwrite |
	lastFileDialog = title;
	auto [it, inserted] = lastPath.try_emplace(lastFileDialog, ".");
	ImGuiFileDialog::Instance()->OpenDialog(
		"FileDialog", title, filters.c_str(), it->second, "", 1, nullptr, flags);
	openFileCallback = callback;
}

void ImGuiOpenFile::paint(MSXMotherBoard* /*motherBoard*/)
{
	// (Modal) file dialog
	auto* fileDialog = ImGuiFileDialog::Instance();
	if (fileDialog->Display("FileDialog", ImGuiWindowFlags_NoCollapse, ImVec2(480.0f, 360.0f))) {
		if (fileDialog->IsOk() && openFileCallback) {
			lastPath[lastFileDialog] = fileDialog->GetCurrentPath();
			std::string filePathName = fileDialog->GetFilePathName();
			openFileCallback(filePathName);
			openFileCallback = {};
		}
		fileDialog->Close();
	}
}

} // namespace openmsx
