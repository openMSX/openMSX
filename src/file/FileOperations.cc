// $Id$

#ifdef	_WIN32
#define WIN32_LEAN_AND_MEAN
#define	_WIN32_IE	0x0400
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#define	MAXPATHLEN	MAX_PATH
#define	mode_t	unsigned short int
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include "ReadDir.hh"
#include "build-info.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "openmsx.hh"
#include "CliComm.hh"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

using std::string;

#ifdef _WIN32

static	struct maint_setenv_table {
	char *env;
	struct maint_setenv_table *next;
} *maint_setenv_table_p = NULL, *maint_setenv_table_c = NULL;

/*
 * A clean up for A tiny setenv() for win32.
 */
static void cleanSetEnv(void)
{
	struct maint_setenv_table* p = maint_setenv_table_p;
	while (p) {
		if (p->env) {
			unsigned len = strlen(p->env);
			for (unsigned i = 0; i < len; ++i) {
				if (p->env[i] == '=') {
					char buf[i + 3];
					strncpy(buf, p->env, i+1);
					buf[i + 2] = '\0';
					// when no string after '=' it works as
					// unset. So buf will be ok even if it
					// is auto variable...
					_putenv(buf);
					break;
				}
			}
			free(p->env);
		}
		struct maint_setenv_table* q = p;
		p = p->next;
		free(q);
	}
}

/*
 * A tiny setenv() for win32.
 */
static int setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite && getenv(name)) {
		return	0;
	} else {
		struct maint_setenv_table *p;
		char *buf;
		p=NULL; buf=NULL;
		if ((p=(struct maint_setenv_table*)malloc(
		                     sizeof(struct maint_setenv_table)))
		    && (buf=(char*)malloc(strlen(name)+strlen(value)+2))) {
			strcpy(buf, name);
			strcat(buf, "=");
			strcat(buf, value);
			if (!_putenv(buf)) {
				p->next = NULL;
				p->env  = buf;
				if (maint_setenv_table_p) {
					maint_setenv_table_c->next = p;
					maint_setenv_table_c = p;
				} else {
					maint_setenv_table_p = p;
					maint_setenv_table_c = p;
					atexit(cleanSetEnv);
				}
				return	0;
			} else {
				return	-1;
			}
		} else {
			if (p) {
				free(p);
			}
			if (buf) {
				free(buf);
			}
			return	-1;
		}
	}
}

#endif // _WIN32


namespace openmsx {

namespace FileOperations {

#ifdef __APPLE__

std::string findShareDir() {
	ProcessSerialNumber psn;
	if (GetCurrentProcess(&psn) != noErr) {
		throw FatalError("Failed to get process serial number");
	}
	FSRef location;
	if (GetProcessBundleLocation(&psn, &location) != noErr) {
		throw FatalError("Failed to get process bundle location");
	}
	FSRef root;
	if (FSPathMakeRef(reinterpret_cast<const UInt8*>("/"), &root, NULL) != noErr) {
		throw FatalError("Failed to get reference to root directory");
	}
	while (true) {
		OSErr err;
		// Are we in the root yet?
		err = FSCompareFSRefs(&location, &root);
		if (err == noErr) {
			throw FatalError("Could not find \"share\" directory anywhere");
		}
		// Go up one level.
		// Initial location is the executable file itself, so the first level up
		// will take us to the directory containing the executable.
		if (FSGetCatalogInfo(
			&location, kFSCatInfoNone, NULL, NULL, NULL, &location
			) != noErr
		) {
			throw FatalError("Failed to get parent directory");
		}

		FSIterator iterator;
		if (FSOpenIterator(&location, kFSIterateFlat, &iterator) != noErr) {
			throw FatalError("Failed to open iterator");
		}
		bool filesLeft = true; // iterator has files left for next call
		while (filesLeft) {
			// Get several files at a time, to reduce the number of system calls.
			const int MAX_SCANNED_FILES = 100;
			ItemCount actualObjects;
			FSRef refs[MAX_SCANNED_FILES];
			FSCatalogInfo catalogInfos[MAX_SCANNED_FILES];
			HFSUniStr255 names[MAX_SCANNED_FILES];
			err = FSGetCatalogInfoBulk(
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
				throw FatalError("Catalog search failed");
			}
			for (ItemCount i = 0; i < actualObjects; i++) {
				// We're only interested in directories.
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
						err = FSCloseIterator(iterator);
						assert(err == noErr);
						// Get full path of directory.
						UInt8 path[256];
						if (FSRefMakePath(&refs[i], path, sizeof(path)) != noErr) {
							throw FatalError("Path too long");
						}
						return std::string(reinterpret_cast<char*>(path));
					}
				}
			}
		}
		err = FSCloseIterator(iterator);
		assert(err == noErr);
	}
}

#endif // __APPLE__

string expandTilde(const string& path)
{
	if ((path.size() <= 1) || (path[0] != '~')) {
		return path;
	}
	if (path[1] == '/') {
		// current user
		return getUserHomeDir() + path.substr(1);
	} else {
		// other user
		std::cerr << "Warning: ~<user>/ not yet implemented" << std::endl;
		return path;
	}
}

void mkdir(const string& path, mode_t mode)
{
#if	defined(__MINGW32__) || defined(_MSC_VER)
	if ((path == "/") ||
	    StringOp::endsWith(path, ":") ||
	    StringOp::endsWith(path, ":/")) {
		return;
	}
	int result = ::mkdir(getNativePath(path).c_str());
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

	struct stat st;
	if ((stat(path.c_str(), &st) != 0) || !S_ISDIR(st.st_mode)) {
		throw FileException("Error creating dir " + path);
	}
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

string getNativePath(const string &path)
{
#if	defined(_WIN32)
	string result(path);
	replace(result.begin(), result.end(), '/', '\\');
	return result;
#else
	return path;
#endif
}

string getConventionalPath(const string &path)
{
#if	defined(_WIN32)
	string result(path);
	replace(result.begin(), result.end(), '\\', '/');
	return result;
#else
	return path;
#endif
}

bool isAbsolutePath(const string& path)
{
#if	defined(_WIN32)
	if ((path.size() >= 3) && (path[1] == ':') && (path[2] == '/')) {
		char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			return true;
		}
	}
#endif
	return !path.empty() && (path[0] == '/');
}

const string& getUserHomeDir()
{
	static string userDir;
	if (userDir.empty()) {
#if	defined(_WIN32)
		HMODULE sh32dll = LoadLibraryA("SHELL32.DLL");
		if (sh32dll) {
			FARPROC funcp = GetProcAddress(sh32dll, "SHGetSpecialFolderPathA");
			if (funcp) {
				char p[MAX_PATH + 1];
				int res = ((BOOL(*)(HWND, LPSTR, int, BOOL))funcp)(0, p, CSIDL_PERSONAL, 1);
				if (res == TRUE) {
					userDir = getConventionalPath(p);
				}
			}
			FreeLibrary(sh32dll);
		}
		if (userDir.empty()) {
			// workaround for Win95 w/o IE4(||later)
			userDir = getSystemDataDir();
			userDir.erase(userDir.length() - 6, 6);	// "/share"
		}
		if ((userDir.length() == 3) && (userDir.substr(1) == ":/")){
			char drive = tolower(userDir[0]);
			if (('a' <= drive) && (drive <= 'z')) {
				userDir.erase(2,1);  // remove the trailing slash because other functions will add it, X:// will be seen as protocol
			}
		}
#else
		userDir = getenv("HOME");
#endif
	}
	return userDir;
}

const string& getUserOpenMSXDir()
{
#if defined(_WIN32)
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
	if (value) {
		return value;
	}

	string newValue = getUserOpenMSXDir() + "/share";
	setenv(NAME, newValue.c_str(), 0);
	return newValue;
}

string getSystemDataDir()
{
	const char* const NAME = "OPENMSX_SYSTEM_DATA";
	char* value = getenv(NAME);
	if (value) {
		return value;
	}

	string newValue;
#if defined(_WIN32)
	char p[MAX_PATH + 1];
	int res = GetModuleFileNameA(NULL, p, MAX_PATH);
	if ((res == 0) || (res == MAX_PATH)) {
		throw FatalError("Cannot detect openMSX directory.");
	}
	if (!strrchr(p, '\\')) {
		throw FatalError("openMSX is not in directory!?");
	}
	*(strrchr(p, '\\')) = '\0';
	newValue = getConventionalPath(p) + "/share";
#elif defined(__APPLE__)
	newValue = findShareDir();
#else
	// defined in build-info.hh (default /opt/openMSX/share)
	newValue = DATADIR;
#endif
	setenv(NAME, newValue.c_str(), 0);
	return newValue;
}

string expandCurrentDirFromDrive (const string& path)
{
	string result = path;
#ifdef _WIN32
	if (((path.size() == 2) && (path[1]==':')) ||
		((path.size() >=3) && (path[1]==':') && (path[2] != '/'))){
		// get current directory for this drive
		unsigned char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <='z')){
			char buffer [MAX_PATH + 1];
			if (_getdcwd(drive - 'a' +1, buffer, MAX_PATH) != NULL){
				result = buffer;
				result = getConventionalPath(result);
				if (result[result.size()-1] != '/'){
					result.append("/");
				}
				if (path.size() >2){
					result.append(path.substr(2));
				}
			}
		}
	}
#endif
	return result;
}

bool isRegularFile(const string& filename)
{
	struct stat st;
	int ret = stat(filename.c_str(), &st);
	return (ret == 0) && S_ISREG(st.st_mode);
}

bool isDirectory(const string& directory)
{
	struct stat st;
	int ret = stat(directory.c_str(), &st);
	return (ret == 0) && S_ISDIR(st.st_mode);
}

bool exists(const string& filename)
{
	struct stat st;
	return stat(filename.c_str(), &st) == 0;
}

static int getNextNum(dirent* d, const string& prefix, const string& extension, const unsigned int nofdigits)
{
	const unsigned int extensionlen = extension.length();
	const unsigned int prefixlen = prefix.length();
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

string getNextNumberedFileName(const string& directory, const string& prefix, const string& extension)
{
	const unsigned int nofdigits = 4;

	int max_num = 0;

	string dirName = getUserOpenMSXDir() + "/" + directory;
	try {
		mkdirp(dirName);
	} catch (FileException& e) {
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

} // namespace FileOperations

} // namespace openmsx
