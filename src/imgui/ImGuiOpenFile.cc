#include "ImGuiOpenFile.hh"

#include "ImGuiManager.hh"

#include "Event.hh"
#include "FileOperations.hh"
#include "MSXException.hh"

#include "StringOp.hh"
#include "narrow.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace openmsx {

struct ImGuiOpenFile::Dialog
{
	// SDL requires these to stay valid until the callback is invoked.
	std::vector<FileFilter> filters;
	std::vector<std::string> sdlPatterns;
	std::vector<SDL_DialogFileFilter> sdlFilters;

	struct Result {
		std::optional<std::string> file; // nullopt when cancelled or on error
		int filter = -1; // index in 'filters', -1 if unknown
		std::string error;
	};
	std::mutex mutex;
	std::optional<Result> result; // set when the dialog is closed
};

#if defined(_WIN32) || defined(__APPLE__)
static constexpr std::string_view errorHint = {};
#else
// SDL's error message doesn't tell how to solve the most likely problem.
static constexpr std::string_view errorHint =
	" (native file dialogs need either 'xdg-desktop-portal' or 'zenity' to be installed)";
#endif

void ImGuiOpenFile::save(ImGuiTextBuffer& buf)
{
	for (const auto& [key, value] : lastPath) {
		buf.appendf("%s=%s\n", key.c_str(), value.c_str());
	}
}

void ImGuiOpenFile::loadLine(std::string_view name, zstring_view value)
{
	if (name == "bookmarks") {
		// written by older versions, bookmarks are no longer supported
		return;
	}
	lastPath[std::string(name)] = value;
}

// Note: SDL's macOS backend shows the union of all filters, and a "*" filter
// disables filtering altogether. So there, the dialogs that get this filter
// show all files. That's better than the alternative: not being able to select
// a file with an unexpected extension.
static void addAllFilesFilter(std::vector<FileFilter>& filters)
{
	if (!contains(filters, "*", &FileFilter::pattern)) {
		filters.push_back({"All files (*)", "*"});
	}
}

// SDL's zenity backend (and before SDL 3.4.6 also the xdg-portal backend)
// matches extensions case-sensitively. So also add upper-case variants:
// "rom;dsk" -> "rom;dsk;ROM;DSK".
[[nodiscard]] static std::string toSdlPattern(std::string_view pattern)
{
	std::string result(pattern);
#if !defined(_WIN32) && !defined(__APPLE__)
	for (auto ext : StringOp::split_view<StringOp::EmptyParts::REMOVE>(pattern, ';')) {
		std::string upper(ext);
		std::ranges::transform(upper, upper.begin(), [](char c) {
			return (('a' <= c) && (c <= 'z')) ? char(c - 'a' + 'A') : c;
		});
		if (upper != ext) strAppend(result, ';', upper);
	}
#endif
	return result;
}

[[nodiscard]] static std::string_view defaultExtension(std::string_view pattern)
{
	if (pattern == "*") return {};
	return StringOp::splitOnFirst(pattern, ';').first;
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
	auto [it, inserted] = lastPath.try_emplace(lastTitle, ".");
	return it->second;
}

// The dialog runs in another process (portal, zenity), so pass an absolute
// path. A trailing separator tells SDL it's a directory rather than a file.
[[nodiscard]] static std::string toDialogLocation(const std::string& dir)
{
	if (dir.empty() || !FileOperations::isDirectory(dir)) return {};
	std::string absolute;
	try {
		absolute = FileOperations::getAbsolutePath(dir);
	} catch (MSXException&) {
		return {};
	}
	if (absolute.ends_with("/.")) absolute.pop_back();
	std::string result(FileOperations::getNativePath(absolute));
	if (!result.ends_with('/') && !result.ends_with('\\')) {
#ifdef _WIN32
		result += '\\';
#else
		result += '/';
#endif
	}
	return result;
}

void ImGuiOpenFile::selectFile(const std::string& title, std::vector<FileFilter> filters,
                               const std::function<void(const std::string&)>& callback,
                               zstring_view lastLocationHint)
{
	common(Type::OPEN_FILE, title, std::move(filters), callback, lastLocationHint);
}

void ImGuiOpenFile::selectNewFile(const std::string& title, std::vector<FileFilter> filters,
                                  const std::function<void(const std::string&)>& callback,
                                  zstring_view lastLocationHint)
{
	common(Type::SAVE_FILE, title, std::move(filters), callback, lastLocationHint);
}

void ImGuiOpenFile::selectDirectory(const std::string& title,
                                    const std::function<void(const std::string&)>& callback,
                                    zstring_view lastLocationHint)
{
	common(Type::OPEN_DIRECTORY, title, {}, callback, lastLocationHint);
}

void ImGuiOpenFile::common(Type type, const std::string& title, std::vector<FileFilter> filters,
                           const std::function<void(const std::string&)>& callback,
                           zstring_view lastLocationHint)
{
	if (pending) return; // a dialog is already open
#ifdef __APPLE__
	// Without a "*" filter, the macOS save panel appends the extension itself
	// (and then confirms overwriting the full name).
	if (type == Type::OPEN_FILE) addAllFilesFilter(filters);
#else
	if (type != Type::OPEN_DIRECTORY) addAllFilesFilter(filters);
#endif

	lastTitle = title;
	openFileCallback = callback;
	pendingType = type;
	showDialog(std::move(filters), toDialogLocation(getStartPath(lastLocationHint)));
}

void ImGuiOpenFile::showDialog(std::vector<FileFilter> filters, const std::string& location)
{
	pending = std::make_shared<Dialog>();
	auto& dialog = *pending;
	dialog.filters = std::move(filters);
	dialog.sdlPatterns = to_vector(std::views::transform(dialog.filters, [](const auto& f) {
		return toSdlPattern(f.pattern);
	}));
	dialog.sdlFilters = to_vector(std::views::transform(
		std::views::zip(dialog.filters, dialog.sdlPatterns), [](const auto& fp) {
			const auto& [f, p] = fp;
			return SDL_DialogFileFilter{f.name.c_str(), p.c_str()};
		}));

	SDL_PropertiesID props = SDL_CreateProperties();
	// Note: macOS probably doesn't show the title (the dialog is a sheet), and
	// SDL has no property for the 'message' that could be shown instead.
	SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, lastTitle.c_str());
	SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER,
	                       SDL_GetWindowFromID(WindowEvent::getMainWindowId()));
	if (!location.empty()) {
		SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.c_str());
	}
	if (!dialog.sdlFilters.empty()) {
		SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, dialog.sdlFilters.data());
		SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, narrow<Sint64>(dialog.sdlFilters.size()));
	}

	auto sdlType = [&] {
		switch (pendingType) {
			case Type::OPEN_FILE:      return SDL_FILEDIALOG_OPENFILE;
			case Type::SAVE_FILE:      return SDL_FILEDIALOG_SAVEFILE;
			case Type::OPEN_DIRECTORY: return SDL_FILEDIALOG_OPENFOLDER;
		}
		UNREACHABLE;
	}();

	// SDL may invoke the callback from another thread, possibly even after
	// this object is destroyed. So the callback only shares 'Dialog'.
	auto* userdata = new std::shared_ptr<Dialog>(pending);
	SDL_ShowFileDialogWithProperties(sdlType,
		[](void* data, const char* const* filelist, int filter) {
			std::unique_ptr<std::shared_ptr<Dialog>> holder(static_cast<std::shared_ptr<Dialog>*>(data));
			Dialog::Result result;
			if (!filelist) {
				const char* err = SDL_GetError();
				result.error = (err && *err) ? err : "unknown error";
			} else if (*filelist && **filelist) {
				// SDL's zenity backend reports cancel as a single empty filename
				result.file = *filelist;
			}
			result.filter = filter;

			auto& d = **holder;
			std::scoped_lock lock(d.mutex);
			d.result = std::move(result);
		},
		userdata, props);
	SDL_DestroyProperties(props);
}

void ImGuiOpenFile::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!pending) return;

	auto result = [&] {
		std::scoped_lock lock(pending->mutex);
		return std::exchange(pending->result, {});
	}();
	if (!result) return; // still open
	auto& [file, filterIdx, error] = *result;
	auto dialog = std::exchange(pending, nullptr);
	auto callback = std::exchange(openFileCallback, {});

	if (!file) {
		if (!error.empty()) {
			manager.printError("Couldn't show file dialog: ", error, errorHint);
		}
		return;
	}

	auto& filters = dialog->filters;
	// Only Windows reports the selected filter, otherwise assume the first one.
	if (filterIdx < 0 || filterIdx >= narrow<int>(filters.size())) filterIdx = 0;

	auto path = FileOperations::getConventionalPath(std::move(*file)); // Windows: replace backslash with slash
	// For a new file without extension, append the extension of the filter.
	// This only checks for 'no extension' (so e.g. "tape.v2" is kept as-is):
	// checking against the extensions of the selected filter is not possible,
	// because only Windows reports which filter was selected.
	if (pendingType == Type::SAVE_FILE && !filters.empty() &&
	    FileOperations::getExtension(FileOperations::getFilename(path)).empty()) {
		if (auto ext = defaultExtension(filters[filterIdx].pattern); !ext.empty()) {
			strAppend(path, '.', ext);
			if (FileOperations::exists(path)) {
				// The dialog only confirmed the name without extension. Show
				// it again with the full name, so that it confirms overwriting.
				openFileCallback = std::move(callback);
				showDialog(std::move(filters), std::string(FileOperations::getNativePath(path)));
				return;
			}
		}
	}
	lastPath[lastTitle] = (pendingType == Type::OPEN_DIRECTORY)
	                    ? path : std::string(FileOperations::getDirName(path));
	if (callback) callback(path);
}

} // namespace openmsx
