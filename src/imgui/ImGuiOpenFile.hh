#ifndef IMGUI_OPEN_FILE_HH
#define IMGUI_OPEN_FILE_HH

#include "ImGuiReadHandler.hh"

#include "TclObject.hh"

#include <functional>
#include <map>
#include <string>

struct ImGuiTextBuffer;

namespace openmsx {

class ImGuiManager;

class ImGuiOpenFile : public ImGuiReadHandler
{
public:
	ImGuiOpenFile(ImGuiManager& manager_)
		: manager(manager_) {}

	void selectFileCommand(const std::string& title, std::string filters, TclObject command);
	void selectFile(const std::string& title, std::string filters,
	                std::function<void(const std::string&)> callback);

	void save(ImGuiTextBuffer& buf);
	void loadLine(std::string_view name, zstring_view value) override;

	void paint();

private:
	ImGuiManager& manager;
	std::map<std::string, std::string> lastPath;
	std::string lastFileDialog;
	std::function<void(const std::string&)> openFileCallback;
};

} // namespace openmsx

#endif
