#ifndef IMGUI_OPEN_FILE_HH
#define IMGUI_OPEN_FILE_HH

#include "ImGuiPart.hh"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

struct FileFilter {
	std::string name;    // e.g. "Disk images (*.dsk)"
	std::string pattern; // e.g. "dsk;gz;zip", or "*" for all files
};

class ImGuiOpenFile final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	// These open a native (OS) file dialog. The callback is invoked later
	// (from paint()) when the user confirmed a selection. Only one dialog
	// can be open at a time: requests made while a dialog is open are
	// ignored. An "All files" filter is added when there's no "*" filter
	// (except for save dialogs on macOS).
	void selectFile(const std::string& title, std::vector<FileFilter> filters,
	                const std::function<void(const std::string&)>& callback,
	                zstring_view lastLocationHint = {});
	void selectNewFile(const std::string& title, std::vector<FileFilter> filters,
	                   const std::function<void(const std::string&)>& callback,
	                   zstring_view lastLocationHint = {});
	void selectDirectory(const std::string& title,
	                     const std::function<void(const std::string&)>& callback,
	                     zstring_view lastLocationHint = {});

	// While a dialog is open, the rest of the GUI and the MSX should not get
	// (mouse/keyboard) input.
	[[nodiscard]] bool isDialogOpen() const { return pending != nullptr; }
	// Stop waiting for the open dialog, its result will be ignored.
	void abandonDialog();

	[[nodiscard]] zstring_view iniName() const override { return "open file dialog"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	struct Dialog;
	enum class Type : uint8_t { OPEN_FILE, SAVE_FILE, OPEN_DIRECTORY };
	void common(Type type, const std::string& title, std::vector<FileFilter> filters,
	            const std::function<void(const std::string&)>& callback,
	            zstring_view lastLocationHint);
	void showDialog(std::vector<FileFilter> filters, const std::string& location);
	void paintWaitOverlay() const;
	[[nodiscard]] std::string getStartPath(zstring_view lastLocationHint);

private:
	std::map<std::string, std::string, std::less<>> lastPath;
	std::string lastTitle;
	std::function<void(const std::string&)> openFileCallback;
	std::shared_ptr<Dialog> pending;
	Type pendingType = Type::OPEN_FILE;
};

} // namespace openmsx

#endif
