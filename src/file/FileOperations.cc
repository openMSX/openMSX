// $Id$

#ifdef	_WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500	// For SHGetSpecialFolderPathW with MinGW
#endif
#include "utf8_checked.hh"
#include "vla.hh"
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#else
#include <sys/types.h>
#include <pwd.h>
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
#include "build-info.hh"
#include <sstream>
#include <cerrno>
#include <cstdlib>

#ifndef _MSC_VER
#include <dirent.h>
#endif

using std::string;

#ifdef _WIN32
using namespace utf8;
#endif

namespace openmsx {

namespace FileOperations {

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
		&catalogInfo, NULL, NULL, &parentRef
		) != noErr) {
		throw FatalError("Failed to get info about bundle path");
	}
	// Get reference to root directory of the volume we are searching.
	// We will need this later to know when to give up.
	FSRef root;
	if (FSGetVolumeInfo(
		catalogInfo.volume, 0, NULL, kFSVolInfoNone, NULL, NULL, &root
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
				NULL /*containerChanged*/,
				kFSCatInfoNodeFlags,
				catalogInfos,
				refs,
				NULL /*specs*/,
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
						if (FSCloseIterator(iterator) != noErr) {
							assert(false);
						}
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
		if (FSCloseIterator(iterator) != noErr) {
			assert(false);
		}
		// Are we in the root yet?
		if (FSCompareFSRefs(&location, &root) == noErr) {
			throw FatalError("Could not find \"share\" directory anywhere");
		}
		// Go up one level.
		if (FSGetCatalogInfo(
			&location, kFSCatInfoNone, NULL, NULL, NULL, &parentRef
			) != noErr
		) {
			throw FatalError("Failed to get parent directory");
		}
		location = parentRef;
	}
}

#endif // __APPLE__

string expandTilde(const string& path)
{
	if (path.empty() || path[0] != '~') {
		return path;
	}
	string::size_type pos = path.find_first_of('/');
	string user = ((path.size() == 1) || (pos == 1)) ? "" :
		path.substr(1, (pos == string::npos) ? pos : pos - 1);
	string result = getUserHomeDir(user);
	if (result.empty()) {
		// failed to find homedir, return the path unchanged
		return path;
	}
	if (pos == string::npos) {
		return result;
	}
	if (*result.rbegin() != '/') {
		result += '/';
	}
	result += path.substr(pos + 1);
	return result;
}

void mkdir(const string& path, mode_t mode)
{
#ifdef _WIN32
	if ((path == "/") ||
		StringOp::endsWith(path, ":") ||
		StringOp::endsWith(path, ":/")) {
		return;
	}
	int result = _wmkdir(utf8to16(getNativePath(path)).c_str());
#else
	int result = ::mkdir(getNativePath(path).c_str(), mode);
#endif
	if (result && (errno != EEXIST)) {
		throw FileException("Error creating dir " + path);
	}
}

void mkdirp(const string& path_)
{
	if (path_.empty()) {
		return;
	}
	string path = expandTilde(path_);

	string::size_type pos = 0;
	do {
		pos = path.find_first_of('/', pos + 1);
		mkdir(path.substr(0, pos), 0755);
	} while (pos != string::npos);

	mode_t mode;
	int result = statGetMode(path, mode);
	if (result != 0 || !S_ISDIR(mode)) {
		throw FileException("Error creating dir " + path);
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

int statGetMode(const string& filename, mode_t& mode)
{
#ifdef _WIN32
	struct _stat st;
	int result = _wstat(utf8to16(filename).c_str(), &st);
#else
	struct stat st;
	int result = stat(filename.c_str(), &st);
#endif
	mode = st.st_mode;
	return result;
}

FILE* openFile(const std::string& filename, const std::string& mode)
{
#ifdef _WIN32
	return _wfopen(
		utf8to16(filename).c_str(),
		utf8to16(mode).c_str());
#else
	return fopen(filename.c_str(), mode.c_str());
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

string getFilename(const string& path)
{
	string::size_type pos = path.rfind('/');
	if (pos == string::npos) {
		return path;
	} else {
		return path.substr(pos + 1);
	}
}

string getBaseName(const string& path)
{
	string::size_type pos = path.rfind('/');
	if (pos == string::npos) {
		return "";
	} else {
		return path.substr(0, pos + 1);
	}
}

string join(const string& part1, const string& part2)
{
	if (isAbsolutePath(part2)) {
		return part2;
	}
	if (part1.empty() || (*part1.rbegin() == '/')) {
		return part1 + part2;
	}
	return part1 + '/' + part2;
}
string join(const string& part1, const string& part2, const string& part3)
{
	return join(join(part1, part2), part3);
}

string join(const string& part1, const string& part2,
			const string& part3, const string& part4)
{
	return join(join(join(part1, part2), part3), part4);
}

string getNativePath(const string& path)
{
#ifdef _WIN32
	string result(path);
	replace(result.begin(), result.end(), '/', '\\');
	return result;
#else
	return path;
#endif
}

string getConventionalPath(const string& path)
{
#ifdef _WIN32
	string result(path);
	replace(result.begin(), result.end(), '\\', '/');
	return result;
#else
	return path;
#endif
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

string getAbsolutePath(const string& path)
{
	// In rare cases getCurrentWorkingDirectory() can throw,
	// so only call it when really necessary.
	if (isAbsolutePath(path)) {
		return path;
	}
	string currentDir = getCurrentWorkingDirectory();
	return join(currentDir, path);
}

bool isAbsolutePath(const string& path)
{
#ifdef _WIN32
	if ((path.size() >= 3) && (path[1] == ':') && (path[2] == '/')) {
		char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			return true;
		}
	}
#endif
	return !path.empty() && (path[0] == '/');
}

string getUserHomeDir(const string& username)
{
#ifdef _WIN32
	(void)(&username); // ignore parameter, avoid warning
	
	wchar_t bufW[MAXPATHLEN + 1];
	if (!SHGetSpecialFolderPathW(NULL, bufW, CSIDL_PERSONAL, TRUE)) {
		throw FatalError("SHGetSpecialFolderPathW failed: " +
			StringOp::toString(GetLastError()));
	}

	return getConventionalPath(utf16to8(bufW));

#elif PLATFORM_GP2X
	return ""; // TODO figure out why stuff below doesn't work
	// We cannot use generic implementation below, because for some
	// reason getpwuid() and getpwnam() cannot be used in statically
	// linked applications.
	const char* dir = getenv("HOME");
	if (!dir) {
		dir = "/root";
	}
	return dir;
#else
	const char* dir = NULL;
	struct passwd* pw = NULL;
	if (username.empty()) {
		dir = getenv("HOME");
		if (!dir) {
			pw = getpwuid(getuid());
		}
	} else {
		pw = getpwnam(username.c_str());
	}
	if (pw) {
		dir = pw->pw_dir;
	}
	return dir ? dir : "";
#endif
}

const string& getUserOpenMSXDir()
{
#ifdef _WIN32
	static const string OPENMSX_DIR = expandTilde("~/openMSX");
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
	char* value = getenv(NAME);
	if (value) {
		return value;
	}

	string newValue;
#ifdef _WIN32
	wchar_t bufW[MAXPATHLEN + 1];
	int res = GetModuleFileNameW(NULL, bufW, sizeof(bufW)/sizeof(bufW[0]));
	if (!res) {
		throw FatalError("Cannot detect openMSX directory. GetModuleFileNameW failed: " +
			StringOp::toString(GetLastError()));
	}

	string filename = utf16to8(bufW);
	string::size_type pos = filename.find_last_of('\\');
	if (pos == string::npos) {
		throw FatalError("openMSX is not in directory!?");
	}
	newValue = getConventionalPath(filename.substr(0, pos)) + "/share";
#elif defined(__APPLE__)
	newValue = findShareDir();
#else
	// defined in build-info.hh (default /opt/openMSX/share)
	newValue = DATADIR;
#endif
	return newValue;
}

string expandCurrentDirFromDrive(const string& path)
{
	string result = path;
#ifdef _WIN32
	if (((path.size() == 2) && (path[1] == ':')) ||
		((path.size() >= 3) && (path[1] == ':') && (path[2] != '/'))) {
		// get current directory for this drive
		unsigned char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			wchar_t bufW[MAXPATHLEN + 1];
			if (_wgetdcwd(drive - 'a' + 1, bufW, MAXPATHLEN) != NULL) {
				result = getConventionalPath(utf16to8(bufW));
				if (*result.rbegin() != '/') {
					result += '/';
				}
				if (path.size() > 2) {
					result += path.substr(2);
				}
			}
		}
	}
#endif
	return result;
}

bool isRegularFile(const string& filename)
{
	mode_t mode;
	int ret = statGetMode(expandTilde(filename), mode);
	return (ret == 0) && S_ISREG(mode);
}

bool isDirectory(const string& directory)
{
	mode_t mode;
	int ret = statGetMode(expandTilde(directory), mode);
	return (ret == 0) && S_ISDIR(mode);
}

bool exists(const string& filename)
{
	mode_t mode;
	int ret = statGetMode(expandTilde(filename), mode);
	return (ret == 0);
}

static int getNextNum(dirent* d, const string& prefix, const string& extension,
					  const unsigned nofdigits)
{
	const size_t extensionlen = extension.length();
	const size_t prefixlen = prefix.length();
	string name(d->d_name);

	if ((name.length() != (prefixlen + nofdigits + extensionlen)) ||
		(name.substr(0, prefixlen) != prefix) ||
		(name.substr(prefixlen + nofdigits, extensionlen) != extension)) {
		return 0;
	}
	string num(name.substr(prefixlen, nofdigits));
	char* endpos;
	unsigned long n = strtoul(num.c_str(), &endpos, 10);
	if (*endpos != '\0') {
		return 0;
	}
	return n;
}

string getNextNumberedFileName(
	const string& directory, const string& prefix, const string& extension)
{
	const unsigned nofdigits = 4;

	int max_num = 0;

	string dirName = getUserOpenMSXDir() + "/" + directory;
	try {
		mkdirp(dirName);
	} catch (FileException&) {
		// ignore
	}

	ReadDir dir(dirName.c_str());
	while (dirent* d = dir.getEntry()) {
		max_num = std::max(max_num, getNextNum(d, prefix, extension, nofdigits));
	}

	std::ostringstream os;
	os << dirName << "/" << prefix;
	os.width(nofdigits);
	os.fill('0');
	os << (max_num + 1) << extension;
	return os.str();
}

string getTempDir()
{
#ifdef _WIN32
	DWORD len = GetTempPathW(0, NULL);
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
	throw FatalError("GetTempPathW failed: " + StringOp::toString(GetLastError()));
#else
	const char* result = NULL;
	if (!result) result = getenv("TMPDIR");
	if (!result) result = getenv("TMP");
	if (!result) result = getenv("TEMP");
	if (!result) {
		result = "/tmp";
	}
	return result;
#endif
}

FILE* openUniqueFile(const std::string& directory, std::string& filename)
{
#ifdef _WIN32
	std::wstring directoryW = utf8to16(directory);
	wchar_t filenameW[MAX_PATH];
	if (!GetTempFileNameW(directoryW.c_str(), L"msx", 0, filenameW)) {
		throw FileException("GetTempFileNameW failed: " +
			StringOp::toString(GetLastError()));
	}
	filename = utf16to8(filenameW);
	FILE* fp = _wfopen(filenameW, L"wb");
#else
	filename = directory + "/XXXXXX";
	int fd = mkstemp(const_cast<char*>(filename.c_str()));
	if (fd == -1) {
		throw FileException("Coundn't get temp file name");
	}
	FILE* fp = fdopen(fd, "wb");
#endif
	return fp;
}

} // namespace FileOperations

} // namespace openmsx
