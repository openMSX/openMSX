#include "FileOperations.hh"

#include "FileException.hh"
#include "ReadDir.hh"

#include "StringOp.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "strCat.hh"
#include "unistdp.hh"

#include "build-info.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iterator>
#include <stdexcept>

#ifdef	_WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500	// For SHGetSpecialFolderPathW with MinGW
#endif
#include "utf8_checked.hh"
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#else // ifdef _WIN32_ ...
#include <climits>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif // ifdef _WIN32_ ... else ...

#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#elifdef MAX_PATH
#define MAXPATHLEN MAX_PATH
#else
#define MAXPATHLEN 4096
#endif

#ifdef __APPLE__
#include "FileOperationsMac.hh"
#endif

#ifndef _MSC_VER
#include <dirent.h>
#endif

#if PLATFORM_ANDROID
#include "SDL_system.h" // for GetExternalStorage stuff
#endif

#ifdef _WIN32
using namespace utf8;
#endif

namespace openmsx::FileOperations {

bool needsTildeExpansion(std::string_view path)
{
	return !path.empty() && (path[0] == '~');
}

std::string expandTilde(std::string path)
{
	if (!needsTildeExpansion(path)) {
		return path;
	}
	auto pos = path.find_first_of('/');
	std::string_view user = ((path.size() == 1) || (pos == 1))
	                ? std::string_view{}
	                : std::string_view(path).substr(1, (pos == std::string::npos) ? pos : pos - 1);
	std::string result = getUserHomeDir(user);
	if (result.empty()) {
		// failed to find homedir, return the path unchanged
		return path;
	}
	if (pos == std::string_view::npos) {
		return result;
	}
	if (result.back() != '/') {
		result += '/';
	}
	std::string_view last = std::string_view(path).substr(pos + 1);
	result.append(last.data(), last.size());
	return result;
}

void mkdir(zstring_view path, mode_t mode)
{
#ifdef _WIN32
	(void)&mode; // Suppress C4100 VC++ warning
	if ((path == "/") || path.ends_with(':') || path.ends_with(":/")) {
		return;
	}
	int result = _wmkdir(utf8to16(getNativePath(std::string(path))).c_str());
#else
	int result = ::mkdir(path.c_str(), mode);
#endif
	if (result && (errno != EEXIST)) {
		throw FileException("Error creating dir ", path);
	}
}

static bool isUNCPath(std::string_view path)
{
#ifdef _WIN32
	return path.starts_with("//") || path.starts_with("\\\\");
#else
	(void)path;
	return false;
#endif
}

void mkdirp(std::string path)
{
	if (path.empty()) {
		return;
	}

	// We may receive platform-specific paths here, so conventionalize early
	path = getConventionalPath(std::move(path));

	// If the directory already exists, don't try to recreate it
	if (isDirectory(path))
		return;

	// If the path is a UNC path (e.g. \\server\share) then the first two paths in the loop below will be \ and \\server
	// If the path is an absolute path (e.g. c:\foo\bar) then the first path in the loop will be C:
	// None of those are valid directory paths, so we skip over them and don't call mkdir.
	// Relative paths are fine, since each segment in the path is significant.
	int skip = isUNCPath(path) ? 2 :
		   isAbsolutePath(path) ? 1 : 0;
	std::string::size_type pos = 0;
	do {
		pos = path.find_first_of('/', pos + 1);
		if (skip) {
			skip--;
			continue;
		}
		mkdir(path.substr(0, pos), 0755);
	} while (pos != std::string::npos);

	if (!isDirectory(path)) {
		throw FileException("Error creating dir ", path);
	}
}

int unlink(zstring_view path)
{
#ifdef _WIN32
	return _wunlink(utf8to16(path).c_str());
#else
	return ::unlink(path.c_str());
#endif
}

int rmdir(zstring_view path)
{
#ifdef _WIN32
	return _wrmdir(utf8to16(path).c_str());
#else
	return ::rmdir(path.c_str());
#endif
}

namespace fs = std::filesystem;

static fs::path makeFsPath(zstring_view path)
{
	#ifdef _WIN32
	    return {utf8to16(path)};
	#else
	    return {path.view()};
	#endif
}

int deleteRecursive(zstring_view path)
{
	std::error_code ec;
	fs::remove_all(makeFsPath(path), ec);
	return ec ? -1 : 0;
}

FILE_t openFile(zstring_view filename, zstring_view mode)
{
	// Mode must contain a 'b' character. On unix this doesn't make any
	// difference. But on windows this is required to open the file
	// in binary mode.
	assert(mode.contains('b'));
#ifdef _WIN32
	return FILE_t(_wfopen(utf8to16(filename).c_str(),
	                      utf8to16(mode).c_str()));
#else
	return FILE_t(fopen(filename.c_str(), mode.c_str()));
#endif
}

void openOfStream(std::ofstream& stream, zstring_view filename)
{
#ifdef _WIN32
	stream.open(utf8to16(filename).c_str());
#else
	stream.open(filename.c_str());
#endif
}

void openOfStream(std::ofstream& stream, zstring_view filename,
                  std::ios_base::openmode mode)
{
#ifdef _WIN32
	stream.open(utf8to16(filename).c_str(), mode);
#else
	stream.open(filename.c_str(), mode);
#endif
}

std::string_view getFilename(std::string_view path)
{
	if (auto pos = path.rfind('/'); pos != std::string_view::npos) {
		return path.substr(pos + 1);
	}
	return path;
}

std::string_view getDirName(std::string_view path)
{
	if (auto pos = path.rfind('/'); pos != std::string_view::npos) {
		return path.substr(0, pos + 1);
	}
	return {};
}

std::string_view getExtension(std::string_view path)
{
	std::string_view filename = getFilename(path);
	if (auto pos = filename.rfind('.'); pos != std::string_view::npos) {
		return filename.substr(pos);
	}
	return {};
}

std::string_view stripExtension(std::string_view path)
{
	if (auto pos = path.rfind('.'); pos != std::string_view::npos) {
		return path.substr(0, pos);
	}
	return path;
}

std::string_view stem(std::string_view path)
{
	auto pos1 = path.find_last_of("/.");
	if (pos1 == std::string_view::npos) {
		return path; // No '/' or '.'
	}
	if (path[pos1] != '.') {
		// filename without extension
		return path.substr(pos1 + 1);
	}
	if (pos1 == 0) { // (the only) '.' is at the start
		return {};
	}

	auto pos2 = path.find_last_of('/', pos1 - 1);
	if (pos2 == std::string_view::npos) {
		return path.substr(0, pos1);
	}
	return path.substr(pos2 + 1, pos1 - pos2 - 1);
}

std::string join(std::string_view part1, std::string_view part2)
{
	if (part1.empty() || isAbsolutePath(part2)) {
		return std::string(part2);
	}
	if (part1.back() == '/') {
		return strCat(part1, part2);
	}
	return strCat(part1, '/', part2);
}
std::string join(std::string_view part1, std::string_view part2, std::string_view part3)
{
	return join(part1, join(part2, part3));
}

std::string join(std::string_view part1, std::string_view part2,
            std::string_view part3, std::string_view part4)
{
	return join(part1, join(part2, join(part3, part4)));
}

#ifdef _WIN32
std::string getNativePath(zstring_view path)
{
	auto result = std::string(path);
	std::ranges::replace(result, '/', '\\');
	return result;
}

std::string getConventionalPath(std::string path)
{
	std::ranges::replace(path, '\\', '/');
	return path;
}
#endif

std::string getCurrentWorkingDirectory()
{
#ifdef _WIN32
	std::array<wchar_t, MAXPATHLEN> bufW;
	const wchar_t* result = _wgetcwd(bufW.data(), narrow<int>(bufW.size()));
	if (!result) {
		throw FileException("Couldn't get current working directory.");
	}
	return utf16to8(result);
#else
	std::array<char, MAXPATHLEN> buf;
	char* result = getcwd(buf.data(), buf.size());
	if (!result) {
		throw FileException("Couldn't get current working directory.");
	}
	return buf.data();
#endif
}

std::string getAbsolutePath(std::string_view path)
{
	// In rare cases getCurrentWorkingDirectory() can throw,
	// so only call it when really necessary.
	if (isAbsolutePath(path)) {
		return std::string(path);
	}
	return join(getCurrentWorkingDirectory(), path);
}

bool isAbsolutePath(std::string_view path)
{
	if (isUNCPath(path)) return true;
#ifdef _WIN32
	if ((path.size() >= 3) && (path[1] == ':') && (path[2] == one_of('/', '\\'))) {
		char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			return true;
		}
	}
#endif
	return !path.empty() && (path[0] == '/');
}

std::string getUserHomeDir(std::string_view username)
{
#ifdef _WIN32
	(void)(&username); // ignore parameter, avoid warning

	std::array<wchar_t, MAXPATHLEN + 1> bufW;
	if (!SHGetSpecialFolderPathW(nullptr, bufW.data(), CSIDL_PERSONAL, TRUE)) {
		throw FatalError(
			"SHGetSpecialFolderPathW failed: ", GetLastError());
	}

	return getConventionalPath(utf16to8(bufW.data()));
#else
	const char* dir = nullptr;
	struct passwd* pw = nullptr;
	if (username.empty()) {
		dir = getenv("HOME");
		if (!dir) {
			pw = getpwuid(getuid());
		}
	} else {
		pw = getpwnam(std::string(username).c_str());
	}
	if (pw) {
		dir = pw->pw_dir;
	}
	return dir ? dir : std::string{};
#endif
}

const std::string& getUserOpenMSXDir()
{
	static const std::string OPENMSX_DIR = []() -> std::string {
		if (const char* home = getenv("OPENMSX_HOME")) {
			return home;
		}
#ifdef _WIN32
		return expandTilde("~/openMSX");
#elif PLATFORM_ANDROID
		// TODO: do something to query whether the storage is available
		// via SDL_AndroidGetExternalStorageState
		return strCat(SDL_AndroidGetExternalStoragePath(), "/openMSX");
#else
		return expandTilde("~/.openMSX");
#endif
	}();
	return OPENMSX_DIR;
}

std::string getUserOpenMSXDir(std::string_view subdir)
{
	return join(getUserOpenMSXDir(), subdir);
}

const std::string& getUserDataDir()
{
	static std::optional<std::string> result;
	if (!result) {
		const char* const NAME = "OPENMSX_USER_DATA";
		const char* value = getenv(NAME);
		result = value ? value : getUserOpenMSXDir("share");
	}
	return *result;
}

const std::string& getSystemDataDir()
{
	static std::optional<std::string> result;
	if (!result) result = []() -> std::string {
		if (const char* value = getenv("OPENMSX_SYSTEM_DATA")) {
			return value;
		}
#ifdef _WIN32
		std::array<wchar_t, MAXPATHLEN + 1> bufW;
		if (int res = GetModuleFileNameW(nullptr, bufW.data(), DWORD(bufW.size()));
		    !res) {
			throw FatalError(
				"Cannot detect openMSX directory. GetModuleFileNameW failed: ",
				GetLastError());
		}

		std::string filename = utf16to8(bufW.data());
		auto pos = filename.find_last_of('\\');
		if (pos == std::string::npos) {
			throw FatalError("openMSX is not in directory!?");
		}
		return getConventionalPath(filename.substr(0, pos)) + "/share";
#elifdef __APPLE__
		return findResourceDir("share");
#elif PLATFORM_ANDROID
		return getAbsolutePath("openmsx_system");
#else
		// defined in build-info.hh (default /opt/openMSX/share)
		return DATADIR;
#endif
	}();
	return *result;
}

const std::string& getSystemDocDir()
{
	static std::optional<std::string> result;
	if (!result) result = []() -> std::string {
#ifdef _WIN32
		std::array<wchar_t, MAXPATHLEN + 1> bufW;
		if (int res = GetModuleFileNameW(nullptr, bufW.data(), DWORD(bufW.size()));
		    !res) {
			throw FatalError(
				"Cannot detect openMSX directory. GetModuleFileNameW failed: ",
				GetLastError());
		}

		std::string filename = utf16to8(bufW.data());
		auto pos = filename.find_last_of('\\');
		if (pos == std::string::npos) {
			throw FatalError("openMSX is not in directory!?");
		}
		return getConventionalPath(filename.substr(0, pos)) + "/doc";
#elifdef __APPLE__
		return findResourceDir("doc");
#elif PLATFORM_ANDROID
		return getAbsolutePath("openmsx_system"); // TODO: currently no docs are installed on Android
#else
		// defined in build-info.hh (default /opt/openMSX/doc)
		return DOCDIR;
#endif
	}();
	return *result;
}

#ifdef _WIN32
static bool driveExists(char driveLetter)
{
	std::array<char, 3> buf = {driveLetter, ':', 0};
	return GetFileAttributesA(buf.data()) != INVALID_FILE_ATTRIBUTES;
}
#endif

#ifdef _WIN32
std::string expandCurrentDirFromDrive(std::string path)
{
	std::string result = path;
	if (((path.size() == 2) && (path[1] == ':')) ||
		((path.size() >= 3) && (path[1] == ':') && (path[2] != '/'))) {
		// get current directory for this drive
		unsigned char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			std::array<wchar_t, MAXPATHLEN + 1> bufW;
			if (driveExists(drive) &&
				_wgetdcwd(drive - 'a' + 1, bufW.data(), MAXPATHLEN)) {
				result = getConventionalPath(utf16to8(bufW.data()));
				if (result.back() != '/') {
					result += '/';
				}
				if (path.size() > 2) {
					auto tmp = std::string_view(path).substr(2);
					result.append(tmp.data(), tmp.size());
				}
			}
		}
	}
	return result;
}
#endif

std::optional<Stat> getStat(zstring_view filename)
{
	std::optional<Stat> st;
	st.emplace(); // allocate relatively large 'struct stat' in the return slot

#ifdef _WIN32
	std::string filename2(filename);
	// workaround for VC++: strip trailing slashes (but keep it if it's the
	// only character in the path)
	if (auto pos = filename2.find_last_not_of('/'); pos != std::string::npos) {
		filename2.resize(pos + 1);
	} else {
		// string was either empty or a (sequence of) '/' character(s)
		if (!filename2.empty()) filename2.resize(1);
	}
	// For some reason, the default _wstat is 32-bit, even though we do not
	// seem to define _USE_32BIT_TIME anywhere... so use explicit 64-bit
	// call.
	if (_wstat64(utf8to16(filename2).c_str(), &*st)) {
		st.reset();
	}
#else
	if (stat(filename.c_str(), &*st)) {
		st.reset();
	}
#endif

	return st; // we count on NRVO to eliminate memcpy of 'struct stat'
}

bool isRegularFile(const Stat& st)
{
	return S_ISREG(st.st_mode);
}
bool isRegularFile(zstring_view filename)
{
	auto st = getStat(filename);
	return st && isRegularFile(*st);
}

bool isDirectory(const Stat& st)
{
	return S_ISDIR(st.st_mode);
}

bool isDirectory(zstring_view directory)
{
	auto st = getStat(directory);
	return st && isDirectory(*st);
}

bool exists(zstring_view filename)
{
	return static_cast<bool>(getStat(filename));
}

static unsigned getNextNum(const dirent* d, std::string_view prefix, std::string_view extension,
                           unsigned nofDigits)
{
	auto extensionLen = extension.size();
	auto prefixLen = prefix.size();
	std::string_view name(d->d_name);

	if ((name.size() != (prefixLen + nofDigits + extensionLen)) ||
	    (!name.starts_with(prefix)) ||
	    (name.substr(prefixLen + nofDigits, extensionLen) != extension)) {
		return 0;
	}
	return StringOp::stringToBase<10, unsigned>(name.substr(prefixLen, nofDigits)).value_or(0);
}

std::string getNextNumberedFileName(
	std::string_view directory, std::string_view prefix, std::string_view extension, bool addSeparator)
{
	std::string newPrefix;
	if (addSeparator) {
		newPrefix = strCat(prefix, (prefix.contains(' ') ? ' ' : '_'));
		prefix = newPrefix;
	}

	const unsigned nofDigits = 4;

	unsigned max_num = 0;

	std::string dirName = getUserOpenMSXDir(directory);
	try {
		mkdirp(dirName);
	} catch (FileException&) {
		// ignore
	}

	ReadDir dir(dirName);
	while (auto* d = dir.getEntry()) {
		max_num = std::max(max_num, getNextNum(d, prefix, extension, nofDigits));
	}

	return std::format("{}{:0{}}{}",
		FileOperations::join(dirName, prefix),
		max_num + 1, nofDigits, extension);
}

std::string parseCommandFileArgument(
	std::string_view argument, std::string_view directory,
	std::string_view prefix,   std::string_view extension)
{
	if (argument.empty()) {
		// directory is also created when needed
		return getNextNumberedFileName(directory, prefix, extension);
	}

	std::string filename(argument);
	if (getDirName(filename).empty()) {
		// no dir given, use standard dir (and create it)
		std::string dir = getUserOpenMSXDir(directory);
		mkdirp(dir);
		filename = join(dir, filename);
	} else {
		filename = expandTilde(std::move(filename));
	}

	if (!filename.ends_with(extension) && !exists(filename)) {
		// Expected extension not already given, append it. But only
		// when the filename without extension doesn't already exist.
		// Without this exception stuff like 'soundlog start /dev/null'
		// reports an error " ... error opening file /dev/null.wav."
		filename.append(extension.data(), extension.size());
	}
	return filename;
}

std::string getTempDir()
{
#ifdef _WIN32
	// Get required buffer size including null terminator.
	auto lenWithNull = GetTempPathW(0, nullptr);
	if (lenWithNull == 0) {
		throw FatalError("GetTempPathW failed: ", GetLastError());
	}

	std::wstring s;
	// Allocate space for characters only (without null terminator)
	s.resize_and_overwrite(lenWithNull - 1, [&](wchar_t* buf, size_t) -> size_t {
		// Returns length excluding the null terminator.
		return GetTempPathW(lenWithNull, buf);
	});

	if ((s.size() > 1) && s.ends_with(L'\\')) s.pop_back();

	return utf16to8(s);
#elif PLATFORM_ANDROID
	return getSystemDataDir() + "/tmp";
#else
	const char* result = nullptr;
	if (!result) result = getenv("TMPDIR");
	if (!result) result = getenv("TMP");
	if (!result) result = getenv("TEMP");
	if (!result) {
		result = "/tmp";
	}
	return result;
#endif
}

FILE_t openUniqueFile(const std::string& directory, std::string& filename)
{
#ifdef _WIN32
	std::wstring directoryW = utf8to16(directory);
	std::array<wchar_t, MAX_PATH> filenameW;
	if (!GetTempFileNameW(directoryW.c_str(), L"msx", 0, filenameW.data())) {
		throw FileException("GetTempFileNameW failed: ", GetLastError());
	}
	filename = utf16to8(filenameW.data());
	return FILE_t(_wfopen(filenameW.data(), L"wb"));
#else
	filename = directory + "/XXXXXX";
	auto oldMask = umask(S_IRWXO | S_IRWXG);
	int fd = mkstemp(filename.data());
	umask(oldMask);
	if (fd == -1) {
		throw FileException("Couldnt get temp file name");
	}
	return FILE_t(fdopen(fd, "wb"));
#endif
}

} // namespace openmsx::FileOperations
