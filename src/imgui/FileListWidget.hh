#ifndef FILE_LIST_WIDGET_HH
#define FILE_LIST_WIDGET_HH

#include "ImGuiUtils.hh"

#include <ctime>
#include <functional>
#include <string>
#include <string_view>

namespace openmsx {

class FileListWidget {
public:
	FileListWidget(std::string_view fileType_, std::string_view extension_, std::string_view directory_);
	// returns whether the menu is open
	bool menu(const char* text);
	// returns whether the menu is open
	bool menu(const char* text, bool enabled);
	void drawTable();

public:
	struct Entry {
		Entry(std::string f, std::time_t t) // workaround, needed for clang, not gcc or msvc
			: fullName(std::move(f)), ftime(t) {} // fixed in clang-16
		std::string fullName;
		std::time_t ftime;
		// this just returns the file name without path and extension
		std::string_view getDefaultDisplayName() const;
	};

	// calls FileOperations::unlink(entry.fullName);
	static void defaultDeleteAction(const Entry& entry);
	// shows tooltip with display name of entry (for long names)
	void defaultHoverAction(const Entry& entry);
	// returns the given filename without its extension

	std::function<void()> drawAction; // default (only) calls drawTable()
	std::function<void(const Entry&)> singleClickAction; // MUST be overwritten
	std::function<void(const Entry&)> hoverAction; // default calls defaultHoverAction
	std::function<void(const Entry&)> doubleClickAction; // default: nothing
	std::function<void(const Entry&)> deleteAction; // default calls defaultDeleteAction
	std::function<imColor(const Entry&)> displayColor; // default: normal color
	std::function<std::string(const Entry&)> displayName; // default: Entry::getDefaultDisplayName()

private:
	void draw();
	void scanDirectory();

private:
	std::string_view fileType;
	std::string_view extension;
	std::string_view directory;
	ConfirmDialog confirmDelete;
	std::vector<Entry> entries;
	bool menuOpen = false;
	bool needSort = false;
};

} // namespace openmsx

#endif
