#include "FileListWidget.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "FileContext.hh"
#include "FileOperations.hh"

#include "foreach_file.hh"

#include <filesystem>

namespace openmsx {

static std::string formatFileTimeFull(std::time_t fileTime)
{
	// Convert time_t to local time (broken-down time in the local time zone)
	std::tm* local_time = std::localtime(&fileTime);

	// Get the local time in human-readable format
	std::stringstream ss;
	ss << std::put_time(local_time, "%F %T");
	return ss.str();
}

static std::string formatFileAbbreviated(std::time_t fileTime)
{
	// Convert time_t to local time (broken-down time in the local time zone)
	std::tm local_time = *std::localtime(&fileTime);

	std::time_t t_now = std::time(nullptr); // get time now
	std::tm now = *std::localtime(&t_now);

	const std::string format = ((now.tm_mday == local_time.tm_mday) &&
					(now.tm_mon  == local_time.tm_mon ) &&
					(now.tm_year == local_time.tm_year)) ? "%T" : "%F";

	// Get the local time in human-readable format
	std::stringstream ss;
	ss << std::put_time(&local_time, format.c_str());
	return ss.str();
};

FileListWidget::FileListWidget(
		std::string_view fileType_, std::string_view extension_, std::string_view directory_)
	: fileType(fileType_), extension(extension_), directory(directory_)
	, confirmDelete(strCat("Confirm delete##filelist-", fileType))
{
	// setup default actions
	drawAction = [&] { drawTable(); };
	hoverAction = [this](const Entry& entry) { defaultHoverAction(entry); };
	deleteAction = [](const Entry& entry) { defaultDeleteAction(entry); };
	displayName = [](const Entry& entry) { return std::string(entry.getDefaultDisplayName()); };
}

bool FileListWidget::menu(const char* text)
{
	menuOpen = im::Menu(text, [&]{ draw(); });
	return menuOpen;
}

bool FileListWidget::menu(const char* text, bool enabled)
{
	menuOpen = im::Menu(text, enabled, [&]{ draw(); });
	return menuOpen;
}

void FileListWidget::draw()
{
	if (!menuOpen) scanDirectory(); // only re-scan on transition from closed -> open
	drawAction(); // calls drawTable() (and possibly does more)
}

void FileListWidget::defaultHoverAction(const Entry& entry)
{
	simpleToolTip(displayName(entry));
}

std::string_view FileListWidget::Entry::getDefaultDisplayName() const
{
	return FileOperations::stem(fullName);
}

void FileListWidget::defaultDeleteAction(const Entry& entry) {
	FileOperations::unlink(entry.fullName);
}

[[nodiscard]] static time_t fileTimeToTimeT(std::filesystem::file_time_type fileTime)
{
	// As we still want to support gcc-11 (and not require 13 yet), we have to use this method
	// as workaround instead of using more modern std::chrono and std::format stuff in all time
	// calculations and formatting.

	// Convert file_time_type to system_clock::time_point (the standard clock since epoch)
	auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		fileTime - std::filesystem::file_time_type::clock::now() +
		std::chrono::system_clock::now());

	// Convert system_clock::time_point to time_t (time since Unix epoch in seconds)
	return std::chrono::system_clock::to_time_t(sctp);
}

void FileListWidget::scanDirectory()
{
	entries.clear();
	for (auto context = userDataFileContext(directory);
	     const auto& path : context.getPaths()) {
		foreach_file(path, [&](const std::string& fullName, std::string_view fileName) {
			if (fileName.ends_with(extension)) {
				std::filesystem::file_time_type ftime = std::filesystem::last_write_time(fullName);
				entries.emplace_back(fullName, fileTimeToTimeT(ftime));
			}
		});
	}
	needSort = true;
}

void FileListWidget::drawTable()
{
	if (entries.empty()) {
		ImGui::TextUnformatted(tmpStrCat("No ", fileType, " files found"));
		return;
	}

	int tableFlags = ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_ContextMenuInBody |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingStretchProp;
	im::Table("##select-filelist", 2, tableFlags, ImVec2(ImGui::GetFontSize() * 25.0f, 240.0f), [&]{
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Date/time", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		// check sort order
		auto* sortSpecs = ImGui::TableGetSortSpecs();
		if (sortSpecs->SpecsDirty || needSort) {
			sortSpecs->SpecsDirty = false;
			needSort = false;
			assert(sortSpecs->SpecsCount == 1);
			assert(sortSpecs->Specs);
			assert(sortSpecs->Specs->SortOrder == 0);

			switch (sortSpecs->Specs->ColumnIndex) {
			case 0: // name
				sortUpDown_String(entries, sortSpecs, &FileListWidget::Entry::getDefaultDisplayName);
				break;
			case 1: // time
				sortUpDown_T(entries, sortSpecs, &FileListWidget::Entry::ftime);
				break;
			default:
				UNREACHABLE;
			}
		}

		for (const auto& entry : entries) {
			if (ImGui::TableNextColumn()) {
				imColor color = displayColor ? displayColor(entry) : imColor::TEXT;
				im::StyleColor(ImGuiCol_Text, getColor(color), [&]{
					if (ImGui::Selectable(displayName(entry).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | (doubleClickAction ? ImGuiSelectableFlags_AllowDoubleClick : 0))) {
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
							doubleClickAction(entry);
						} else {
							singleClickAction(entry);
						}
					}
				});
				if (hoverAction && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					hoverAction(entry);
				}
				if (deleteAction) {
					im::PopupContextItem([&]{
						if (ImGui::MenuItem("delete")) {
							confirmDelete.open(
								strCat("Delete ", fileType, " file '", entry.getDefaultDisplayName(), "'?"),
								[entry, this]{
									deleteAction(entry);
									scanDirectory();
								});
						}
					});
				}
			}
			if (ImGui::TableNextColumn()) {
				ImGui::TextUnformatted(formatFileAbbreviated(entry.ftime));
				simpleToolTip([&] { return formatFileTimeFull(entry.ftime); });
			}
	}});
	confirmDelete.execute();
}

} // namespace openmsx
