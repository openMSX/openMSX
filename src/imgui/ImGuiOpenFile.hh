#ifndef IMGUI_OPEN_FILE_HH
#define IMGUI_OPEN_FILE_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <functional>
#include <map>
#include <string>

namespace openmsx {

class ImGuiManager;

class ImGuiOpenFile final : public ImGuiPart
{
public:
	enum class Painter {
		MANAGER,
		DISKMANIPULATOR,
	};

	ImGuiOpenFile(ImGuiManager& manager_)
		: manager(manager_) {}

	void selectFile(const std::string& title, std::string filters,
	                std::function<void(const std::string&)> callback,
	                zstring_view lastLocationHint = {},
			Painter painter = Painter::MANAGER);
	void selectNewFile(const std::string& title, std::string filters,
	                   std::function<void(const std::string&)> callback,
	                   zstring_view lastLocationHint = {},
	                   Painter painter = Painter::MANAGER);
	void selectDirectory(const std::string& title,
	                     std::function<void(const std::string&)> callback,
	                     zstring_view lastLocationHint = {},
	                     Painter painter = Painter::MANAGER);

	// should be called from within a callback of the methods above
	static std::string getLastFilter();

	[[nodiscard]] zstring_view iniName() const override { return "open file dialog"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;

	[[nodiscard]] bool mustPaint(Painter p) const { return p == activePainter; }
	void doPaint();

private:
	void common(const std::string& title, const char* filters,
                    std::function<void(const std::string&)> callback,
                    zstring_view lastLocationHint,
                    Painter painter,
                    int extraFlags);
	void setBookmarks();
	[[nodiscard]] std::string getStartPath(zstring_view lastLocationHint);

private:
	ImGuiManager& manager;
	std::map<std::string, std::string> lastPath;
	std::string lastTitle;
	std::function<void(const std::string&)> openFileCallback;
	Painter activePainter;
	bool chooseDirectory = false;
};

} // namespace openmsx

#endif
