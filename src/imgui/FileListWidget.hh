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
	void menu(const char* text);
	void menu(const char* text, bool enabled);
	void drawTable();

public:
	struct Entry {
		Entry(std::string f, std::string d, std::time_t t) // workaround, needed for clang, not gcc or msvc
			: fullName(std::move(f)), displayName(std::move(d)), ftime(t) {} // fixed in clang-16
		std::string fullName;
		std::string displayName;
		std::time_t ftime;
	};

	std::function<void()> drawAction; // default (only) calls drawTable()
	std::function<void(const Entry&)> selectAction; // MUST be overwritten
	std::function<void(const Entry&)> hoverAction; // default: nothing
	std::function<void(const Entry&)> deleteAction; // default calls FileOperations::unlink(entry.fullName);

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
