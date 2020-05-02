#ifdef	_WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500	// For SHGetSpecialFolderPathW with MinGW
#endif
#include "utf8_checked.hh"
#include "vla.hh"
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
#include <sys/types.h>
#include <pwd.h>
#include <climits>
#include <unistd.h>
#endif // ifdef _WIN32_ ... else ...

#include "openmsx.hh" // for ad_printf

#include "systemfuncs.hh"

#if HAVE_NFTW
#include <ftw.h>
#endif

#if defined(PATH_MAX)
#define MAXPATHLEN PATH_MAX
#elif defined(MAX_PATH)
#define MAXPATHLEN MAX_PATH
#else
#define MAXPATHLEN 4096
#endif


#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include "ReadDir.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include "statp.hh"
#include "unistdp.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "build-info.hh"
#include <algorithm>
#include <sstream>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <cassert>
#include <iterator>

#ifndef _MSC_VER
#include <dirent.h>
#endif

#if PLATFORM_ANDROID
#include "SDL_system.h" // for GetExternalStorage stuff
#endif

using std::string;
using std::string_view;

#ifdef _WIN32
using namespace utf8;
#endif

namespace openmsx::FileOperations {

#ifdef __APPLE__

static std::string findShareDir()
{
	// Find bundle location:
	// for an app folder, this is the outer directory,
	// for an unbundled executable, it is the executable itself.
	ProcessSerialNumber psn;
	if (GetCurrentProcess(&psn) != noErr) {
		throw FatalError("Failed to get process serial number");
	}
	FSRef location;
	if (GetProcessBundleLocation(&psn, &location) != noErr) {
		throw FatalError("Failed to get process bundle location");
	}
	// Get info about the location.
	FSCatalogInfo catalogInfo;
	FSRef parentRef;
	if (FSGetCatalogInfo(
		&location, kFSCatInfoVolume | kFSCatInfoNodeFlags,
		&catalogInfo, nullptr, nullptr, &parentRef
		) != noErr) {
		throw FatalError("Failed to get info about bundle path");
	}
	// Get reference to root directory of the volume we are searching.
	// We will need this later to know when to give up.
	FSRef root;
	if (FSGetVolumeInfo(
		catalogInfo.volume, 0, nullptr, kFSVolInfoNone, nullptr, nullptr, &root
		) != noErr) {
		throw FatalError("Failed to get reference to root directory");
	}
	// Make sure we are looking at a directory.
	if (~catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) {
		// Location is not a directory, so it is the path to the executable.
		location = parentRef;
	}
	while (true) {
		// Iterate through the files in the directory.
		FSIterator iterator;
		if (FSOpenIterator(&location, kFSIterateFlat, &iterator) != noErr) {
			throw FatalError("Failed to open iterator");
		}
		bool filesLeft = true; // iterator has files left for next call
		while (filesLeft) {
			// Get info about several files at a time.
			const int MAX_SCANNED_FILES = 100;
			ItemCount actualObjects;
			FSRef refs[MAX_SCANNED_FILES];
			FSCatalogInfo catalogInfos[MAX_SCANNED_FILES];
			HFSUniStr255 names[MAX_SCANNED_FILES];
			OSErr err = FSGetCatalogInfoBulk(
				iterator,
				MAX_SCANNED_FILES,
				&actualObjects,
				nullptr /*containerChanged*/,
				kFSCatInfoNodeFlags,
				catalogInfos,
				refs,
				nullptr /*specs*/,
				names
				);
			if (err == errFSNoMoreItems) {
				filesLeft = false;
			} else if (err != noErr) {
				throw FatalError("Catalog get failed");
			}
			for (ItemCount i = 0; i < actualObjects; i++) {
				// We're only interested in subdirectories.
				if (catalogInfos[i].nodeFlags & kFSNodeIsDirectoryMask) {
					// Convert the name to a CFString.
					CFStringRef name = CFStringCreateWithCharactersNoCopy(
						kCFAllocatorDefault,
						names[i].unicode,
						names[i].length,
						kCFAllocatorNull // do not deallocate character buffer
						);
					// Is this the directory we are looking for?
					static const CFStringRef SHARE = CFSTR("share");
					CFComparisonResult cmp = CFStringCompare(SHARE, name, 0);
					CFRelease(name);
					if (cmp == kCFCompareEqualTo) {
						// Clean up.
						OSErr closeErr = FSCloseIterator(iterator);
						assert(closeErr == noErr); (void)closeErr;
						// Get full path of directory.
						UInt8 path[256];
						if (FSRefMakePath(
							&refs[i], path, sizeof(path)) != noErr
							) {
							throw FatalError("Path too long");
						}
						return std::string(reinterpret_cast<char*>(path));
					}
				}
			}
		}
		OSErr closeErr = FSCloseIterator(iterator);
		assert(closeErr == noErr); (void)closeErr;
		// Are we in the root yet?
		if (FSCompareFSRefs(&location, &root) == noErr) {
			throw FatalError("Could not find \"share\" directory anywhere");
		}
		// Go up one level.
		if (FSGetCatalogInfo(
			&location, kFSCatInfoNone, nullptr, nullptr, nullptr, &parentRef
			) != noErr
		) {
			throw FatalError("Failed to get parent directory");
		}
		location = parentRef;
	}
}

#endif // __APPLE__

string expandTilde(string_view path)
{
	if (path.empty() || path[0] != '~') {
		return string(path);
	}
	auto pos = path.find_first_of('/');
	string_view user = ((path.size() == 1) || (pos == 1))
	                ? string_view{}
	                : path.substr(1, (pos == string_view::npos) ? pos : pos - 1);
	string result = getUserHomeDir(user);
	if (result.empty()) {
		// failed to find homedir, return the path unchanged
		return string(path);
	}
	if (pos == string_view::npos) {
		return result;
	}
	if (result.back() != '/') {
		result += '/';
	}
	string_view last = path.substr(pos + 1);
	result.append(last.data(), last.size());
	return result;
}

void mkdir(const string& path, mode_t mode)
{
#ifdef _WIN32
	(void)&mode; // Suppress C4100 VC++ warning
	if ((path == "/") ||
		StringOp::endsWith(path, ':') ||
		StringOp::endsWith(path, ":/")) {
		return;
	}
	int result = _wmkdir(utf8to16(getNativePath(path)).c_str());
#else
	int result = ::mkdir(path.c_str(), mode);
#endif
	if (result && (errno != EEXIST)) {
		throw FileException("Error creating dir ", path);
	}
}

static bool isUNCPath(string_view path)
{
#ifdef _WIN32
	return StringOp::startsWith(path, "//") || StringOp::startsWith(path, "\\\\");
#else
	(void)path;
	return false;
#endif
}

void mkdirp(string_view path_)
{
	if (path_.empty()) {
		return;
	}

	// We may receive platform-specific paths here, so conventionalize early
	string path = getConventionalPath(expandTilde(path_));

	// If the directory already exists, don't try to recreate it
	if (isDirectory(path))
		return;

	// If the path is a UNC path (e.g. \\server\share) then the first two paths in the loop below will be \ and \\server
	// If the path is an absolute path (e.g. c:\foo\bar) then the first path in the loop will be C:
	// None of those are valid directory paths, so we skip over them and don't call mkdir.
	// Relative paths are fine, since each segment in the path is significant.
	int skip = isUNCPath(path) ? 2 :
		   isAbsolutePath(path) ? 1 : 0;
	string::size_type pos = 0;
	do {
		pos = path.find_first_of('/', pos + 1);
		if (skip) {
			skip--;
			continue;
		}
		mkdir(path.substr(0, pos), 0755);
	} while (pos != string::npos);

	if (!isDirectory(path)) {
		throw FileException("Error creating dir ", path);
	}
}

int unlink(const std::string& path)
{
#ifdef _WIN32
	return _wunlink(utf8to16(path).c_str());
#else
	return ::unlink(path.c_str());
#endif
}

int rmdir(const std::string& path)
{
#ifdef _WIN32
	return _wrmdir(utf8to16(path).c_str());
#else
	return ::rmdir(path.c_str());
#endif
}

#ifdef _WIN32
int deleteRecursive(const std::string& path)
{
	std::wstring pathW = utf8to16(path);

	SHFILEOPSTRUCTW rmdirFileop;
	rmdirFileop.hwnd = nullptr;
	rmdirFileop.wFunc = FO_DELETE;
	rmdirFileop.pFrom = pathW.c_str();
	rmdirFileop.pTo = nullptr;
	rmdirFileop.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
	rmdirFileop.fAnyOperationsAborted = FALSE;
	rmdirFileop.hNameMappings = nullptr;
	rmdirFileop.lpszProgressTitle = nullptr;

	return SHFileOperationW(&rmdirFileop);
}
#elif HAVE_NFTW
static int deleteRecursive_cb(const char* fpath, const struct stat* /*sb*/,
                              int /*typeflag*/, struct FTW* /*ftwbuf*/)
{
	return remove(fpath);
}
int deleteRecursive(const std::string& path)
{
	return nftw(path.c_str(), deleteRecursive_cb, 64, FTW_DEPTH | FTW_PHYS);
}
#else
// This is a platform independent version of deleteRecursive() (it builds on
// top of helper routines that _are_ platform specific). Though I still prefer
// the two platform specific deleteRecursive() routines above because they are
// likely more optimized and likely contain less bugs than this version (e.g.
// we're walking over the entries in a directory while simultaneously deleting
// entries in that same directory. Although this seems to work fine, I'm not
// 100% sure our ReadDir 'emulation code' for windows covers all corner cases.
// While the windows version above very likely does handle everything).
int deleteRecursive(const std::string& path)
{
	if (isDirectory(path)) {
		{
			ReadDir dir(path);
			while (dirent* d = dir.getEntry()) {
				int err = deleteRecursive(d->d_name);
				if (err) return err;
			}
		}
		return rmdir(path);
	} else {
		return unlink(path);
	}
}
#endif

FILE_t openFile(const std::string& filename, const std::string& mode)
{
	// Mode must contain a 'b' character. On unix this doesn't make any
	// difference. But on windows this is required to open the file
	// in binary mode.
	assert(mode.find('b') != std::string::npos);
#ifdef _WIN32
	return FILE_t(_wfopen(utf8to16(filename).c_str(),
	                      utf8to16(mode).c_str()));
#else
	return FILE_t(fopen(filename.c_str(), mode.c_str()));
#endif
}

void openofstream(std::ofstream& stream, const std::string& filename)
{
#if defined _WIN32 && defined _MSC_VER
	// MinGW 3.x doesn't support ofstream.open(wchar_t*)
	// TODO - this means that unicode text may not work right here
	stream.open(utf8to16(filename).c_str());
#else
	stream.open(filename.c_str());
#endif
}

void openofstream(std::ofstream& stream, const std::string& filename,
				  std::ios_base::openmode mode)
{
#if defined _WIN32 && defined _MSC_VER
	// MinGW 3.x doesn't support ofstream.open(wchar_t*)
	// TODO - this means that unicode text may not work right here
	stream.open(utf8to16(filename).c_str(), mode);
#else
	stream.open(filename.c_str(), mode);
#endif
}

string_view getFilename(string_view path)
{
	auto pos = path.rfind('/');
	if (pos == string_view::npos) {
		return path;
	} else {
		return path.substr(pos + 1);
	}
}

string_view getDirName(string_view path)
{
	auto pos = path.rfind('/');
	if (pos == string_view::npos) {
		return {};
	} else {
		return path.substr(0, pos + 1);
	}
}

string_view getExtension(string_view path)
{
	string_view filename = getFilename(path);
	auto pos = filename.rfind('.');
	if (pos == string_view::npos) {
		return string_view();
	} else {
		return filename.substr(pos);
	}
}

string_view stripExtension(string_view path)
{
	auto pos = path.rfind('.');
	if (pos == string_view::npos) {
		return path;
	} else {
		return path.substr(0, pos);
	}
}

string join(string_view part1, string_view part2)
{
	if (part1.empty() || isAbsolutePath(part2)) {
		return string(part2);
	}
	if (part1.back() == '/') {
		return strCat(part1, part2);
	}
	return strCat(part1, '/', part2);
}
string join(string_view part1, string_view part2, string_view part3)
{
	return join(part1, join(part2, part3));
}

string join(string_view part1, string_view part2,
            string_view part3, string_view part4)
{
	return join(part1, join(part2, join(part3, part4)));
}

string getNativePath(string_view path)
{
	string result(path);
#ifdef _WIN32
	ranges::replace(result, '/', '\\');
#endif
	return result;
}

string getConventionalPath(string_view path)
{
	string result(path);
#ifdef _WIN32
	ranges::replace(result, '\\', '/');
#endif
	return result;
}

string getCurrentWorkingDirectory()
{
#ifdef _WIN32
	wchar_t bufW[MAXPATHLEN];
	wchar_t* result = _wgetcwd(bufW, MAXPATHLEN);
	string buf;
	if (result) {
		buf = utf16to8(result);
	}
#else
	char buf[MAXPATHLEN];
	char* result = getcwd(buf, MAXPATHLEN);
#endif
	if (!result) {
		throw FileException("Couldn't get current working directory.");
	}
	return buf;
}

string getAbsolutePath(string_view path)
{
	// In rare cases getCurrentWorkingDirectory() can throw,
	// so only call it when really necessary.
	if (isAbsolutePath(path)) {
		return string(path);
	}
	string currentDir = getCurrentWorkingDirectory();
	return join(currentDir, path);
}

bool isAbsolutePath(string_view path)
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

string getUserHomeDir(string_view username)
{
#ifdef _WIN32
	(void)(&username); // ignore parameter, avoid warning

	wchar_t bufW[MAXPATHLEN + 1];
	if (!SHGetSpecialFolderPathW(nullptr, bufW, CSIDL_PERSONAL, TRUE)) {
		throw FatalError(
			"SHGetSpecialFolderPathW failed: ", GetLastError());
	}

	return getConventionalPath(utf16to8(bufW));
#else
	const char* dir = nullptr;
	struct passwd* pw = nullptr;
	if (username.empty()) {
		dir = getenv("HOME");
		if (!dir) {
			pw = getpwuid(getuid());
		}
	} else {
		pw = getpwnam(string(username).c_str());
	}
	if (pw) {
		dir = pw->pw_dir;
	}
	return dir ? dir : string{};
#endif
}

const string& getUserOpenMSXDir()
{
#ifdef _WIN32
	static const string OPENMSX_DIR = expandTilde("~/openMSX");
#elif PLATFORM_ANDROID
	// TODO: do something to query whether the storage is available
	// via SDL_AndroidGetExternalStorageState
	static const string OPENMSX_DIR = strCat(SDL_AndroidGetExternalStoragePath(), "/openMSX");
#else
	static const string OPENMSX_DIR = expandTilde("~/.openMSX");
#endif
	return OPENMSX_DIR;
}

string getUserDataDir()
{
	const char* const NAME = "OPENMSX_USER_DATA";
	char* value = getenv(NAME);
	return value ? value : getUserOpenMSXDir() + "/share";
}

string getSystemDataDir()
{
	const char* const NAME = "OPENMSX_SYSTEM_DATA";
	if (char* value = getenv(NAME)) {
		return value;
	}

	string newValue;
#ifdef _WIN32
	wchar_t bufW[MAXPATHLEN + 1];
	int res = GetModuleFileNameW(nullptr, bufW, std::size(bufW));
	if (!res) {
		throw FatalError(
			"Cannot detect openMSX directory. GetModuleFileNameW failed: ",
			GetLastError());
	}

	string filename = utf16to8(bufW);
	auto pos = filename.find_last_of('\\');
	if (pos == string::npos) {
		throw FatalError("openMSX is not in directory!?");
	}
	newValue = getConventionalPath(filename.substr(0, pos)) + "/share";
#elif defined(__APPLE__)
	newValue = findShareDir();
#elif PLATFORM_ANDROID
	newValue = getAbsolutePath("openmsx_system");
	ad_printf("System data dir: %s", newValue.c_str());
#else
	// defined in build-info.hh (default /opt/openMSX/share)
	newValue = DATADIR;
#endif
	return newValue;
}

#ifdef _WIN32
static bool driveExists(char driveLetter)
{
	char buf[] = { driveLetter, ':', 0 };
	return GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES;
}
#endif

string expandCurrentDirFromDrive(string_view path)
{
	string result(path);
#ifdef _WIN32
	if (((path.size() == 2) && (path[1] == ':')) ||
		((path.size() >= 3) && (path[1] == ':') && (path[2] != '/'))) {
		// get current directory for this drive
		unsigned char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			wchar_t bufW[MAXPATHLEN + 1];
			if (driveExists(drive) &&
				_wgetdcwd(drive - 'a' + 1, bufW, MAXPATHLEN)) {
				result = getConventionalPath(utf16to8(bufW));
				if (result.back() != '/') {
					result += '/';
				}
				if (path.size() > 2) {
					string_view tmp = path.substr(2);
					result.append(tmp.data(), tmp.size());
				}
			}
		}
	}
#endif
	return result;
}

bool getStat(string_view filename_, Stat& st)
{
	string filename = expandTilde(filename_);
	// workaround for VC++: strip trailing slashes (but keep it if it's the
	// only character in the path)
	auto pos = filename.find_last_not_of('/');
	if (pos == string::npos) {
		// string was either empty or a (sequence of) '/' character(s)
		filename = filename.empty() ? string{} : "/";
	} else {
		filename.resize(pos + 1);
	}
#ifdef _WIN32
	return _wstat(utf8to16(filename).c_str(), &st) == 0;
#else
	return stat(filename.c_str(), &st) == 0;
#endif
}

bool isRegularFile(const Stat& st)
{
	return S_ISREG(st.st_mode);
}
bool isRegularFile(string_view filename)
{
	Stat st;
	return getStat(filename, st) && isRegularFile(st);
}

bool isDirectory(const Stat& st)
{
	return S_ISDIR(st.st_mode);
}

bool isDirectory(string_view directory)
{
	Stat st;
	return getStat(directory, st) && isDirectory(st);
}

bool exists(string_view filename)
{
	Stat st; // dummy
	return getStat(filename, st);
}

time_t getModificationDate(const Stat& st)
{
	return st.st_mtime;
}

static unsigned getNextNum(dirent* d, string_view prefix, string_view extension,
                           unsigned nofdigits)
{
	auto extensionLen = extension.size();
	auto prefixLen = prefix.size();
	string_view name(d->d_name);

	if ((name.size() != (prefixLen + nofdigits + extensionLen)) ||
	    (name.substr(0, prefixLen) != prefix) ||
	    (name.substr(prefixLen + nofdigits, extensionLen) != extension)) {
		return 0;
	}
	try {
		return StringOp::fast_stou(name.substr(prefixLen, nofdigits));
	} catch (std::invalid_argument&) {
		return 0;
	}
}

string getNextNumberedFileName(
	string_view directory, string_view prefix, string_view extension)
{
	const unsigned nofdigits = 4;

	unsigned max_num = 0;

	string dirName = strCat(getUserOpenMSXDir(), '/', directory);
	try {
		mkdirp(dirName);
	} catch (FileException&) {
		// ignore
	}

	ReadDir dir(dirName);
	while (auto* d = dir.getEntry()) {
		max_num = std::max(max_num, getNextNum(d, prefix, extension, nofdigits));
	}

	std::ostringstream os;
	os << dirName << '/' << prefix;
	os.width(nofdigits);
	os.fill('0');
	os << (max_num + 1) << extension;
	return os.str();
}

string parseCommandFileArgument(
	string_view argument, string_view directory,
	string_view prefix,   string_view extension)
{
	if (argument.empty()) {
		// directory is also created when needed
		return getNextNumberedFileName(directory, prefix, extension);
	}

	string filename(argument);
	if (getDirName(filename).empty()) {
		// no dir given, use standard dir (and create it)
		string dir = strCat(getUserOpenMSXDir(), '/', directory);
		mkdirp(dir);
		filename = strCat(dir, '/', filename);
	} else {
		filename = expandTilde(filename);
	}

	if (!StringOp::endsWith(filename, extension) &&
	    !exists(filename)) {
		// Expected extension not already given, append it. But only
		// when the filename without extension doesn't already exist.
		// Without this exception stuff like 'soundlog start /dev/null'
		// reports an error " ... error opening file /dev/null.wav."
		filename.append(extension.data(), extension.size());
	}
	return filename;
}

string getTempDir()
{
#ifdef _WIN32
	DWORD len = GetTempPathW(0, nullptr);
	if (len) {
		VLA(wchar_t, bufW, (len+1));
		len = GetTempPathW(len, bufW);
		if (len) {
			// Strip last backslash
			if (bufW[len-1] == L'\\') {
				bufW[len-1] = L'\0';
			}
			return utf16to8(bufW);
		}
	}
	throw FatalError("GetTempPathW failed: ", GetLastError());
#elif PLATFORM_ANDROID
	string result = getSystemDataDir() + "/tmp";
	return result;
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
	wchar_t filenameW[MAX_PATH];
	if (!GetTempFileNameW(directoryW.c_str(), L"msx", 0, filenameW)) {
		throw FileException("GetTempFileNameW failed: ", GetLastError());
	}
	filename = utf16to8(filenameW);
	return FILE_t(_wfopen(filenameW, L"wb"));
#else
	filename = directory + "/XXXXXX";
	auto oldMask = umask(S_IRWXO | S_IRWXG);
	int fd = mkstemp(const_cast<char*>(filename.c_str()));
	umask(oldMask);
	if (fd == -1) {
		throw FileException("Coundn't get temp file name");
	}
	return FILE_t(fdopen(fd, "wb"));
#endif
}

} // namespace openmsx::FileOperations
