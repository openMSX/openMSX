#include "ImGuiOpenFile.hh"
#include "ImGuiManager.hh"

#include <imgui.h>
#include <ImGuiFileDialog.h>

namespace openmsx {

void ImGuiOpenFile::save(ImGuiTextBuffer& buf)
{
	buf.append("[openmsx][open file dialog]\n");
	for (const auto& [key, value] : lastPath) {
		buf.appendf("%s=%s\n", key.c_str(), value.c_str());
	}
	buf.append("\n");
}

void ImGuiOpenFile::loadLine(std::string_view name, zstring_view value)
{
	lastPath[std::string(name)] = value;
}

void ImGuiOpenFile::selectFileCommand(const std::string& title, std::string filters, TclObject command)
{
	selectFile(title, std::move(filters), [this, command](const std::string& filename) mutable {
		command.addListElement(filename);
		manager.executeDelayed(command);
	});
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

void ImGuiOpenFile::paint()
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
