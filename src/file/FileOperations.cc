// $Id$

#ifdef	__WIN32__
#define WIN32_LEAN_AND_MEAN
#define	_WIN32_IE	0x0400
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#include <ctype.h>
#include <algorithm>
#define	MAXPATHLEN	MAX_PATH
#define	mode_t	unsigned short int
#endif
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "FileOperations.hh"
#include "openmsx.hh"
#include "CliCommOutput.hh"
#include "MSXException.hh"


namespace openmsx {

/* A wrapper for mkdir().  On some systems, mkdir() does not take permision in
 * arguments. For such systems, in this function, adjust arguments.
 */
static int doMkdir(const char* name, mode_t mode)
{
#if	defined(__MINGW32__) || defined(_MSC_VER)
	if ((name[0]=='/' || name[0]=='\\') && name[1]=='\0' || 
	    (name[1]==':' && name[3]=='\0' && (name[2]=='/' || name[2]=='\\')
	    && ((name[0]>='A' && name[0]<='Z')||(name[0]>='a' && name[0]<='z')))) {
		// *(_errno()) = EEXIST;
		// return -1;
		return 0;
	} else {
		return mkdir(name);
	}
#else
	return mkdir(name, mode);
#endif
}


string FileOperations::expandTilde(const string& path)
{
	if ((path.size() <= 1) || (path[0] != '~')) {
		return path;
	}
	if (path[1] == '/') {
		// current user
		return getUserHomeDir() + path.substr(1);
	} else {
		// other user
		CliCommOutput::instance().printWarning(
			"~<user>/ not yet implemented");
		return path;
	}
}


void FileOperations::mkdirp(const string& path_) throw (FileException)
{
	if (path_.empty()) {
		return;
	}
	string path = expandTilde(path_);

	unsigned pos = 0;
	do {
		pos = path.find_first_of('/', pos + 1);
		if (doMkdir(getNativePath(path).substr(0, pos).c_str(), 0755) &&
		    (errno != EEXIST)) {
			throw FileException("Error creating dir " + path);
		}
	} while (pos != string::npos);

	struct stat st;
	if ((stat(path.c_str(), &st) != 0) || !S_ISDIR(st.st_mode)) {
		throw FileException("Error creating dir " + path);
	}
}

string FileOperations::getFilename(const string& path)
{
	unsigned pos = path.rfind('/');
	if (pos == string::npos) {
		return path;
	} else {
		return path.substr(pos + 1);
	}
}

string FileOperations::getBaseName(const string& path)
{
	unsigned pos = path.rfind('/');
	if (pos == string::npos) {
		return "";
	} else {
		return path.substr(0, pos + 1);
	}
}

string FileOperations::getNativePath(const string &path)
{
#if	defined(__WIN32__)
	string result(path);
	replace(result.begin(), result.end(), '/', '\\');
	return result;
#else
	return path;
#endif
}

string FileOperations::getConventionalPath(const string &path)
{
#if	defined(__WIN32__)
	string result(path);
	replace(result.begin(), result.end(), '\\', '/');
	return result;
#else
	return path;
#endif
}

bool FileOperations::isAbsolutePath(const string& path)
{
#if	defined(__WIN32__)
	if ((path.size() >= 3) && (path[1] == ':') && (path[2] == '/')) {
		char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <= 'z')) {
			return true;
		}
	}
#endif
	return !path.empty() && (path[0] == '/');
}

const string& FileOperations::getUserHomeDir()
{
	static string userDir;
	if (userDir.empty()) {
#if	defined(__WIN32__)
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
#else
		userDir = getenv("HOME");
#endif
	}
	return userDir;
}

const string& FileOperations::getUserOpenMSXDir()
{
#if defined(__WIN32__)
	static const string OPENMSX_DIR = expandTilde("~/openMSX");
#else
	static const string OPENMSX_DIR = expandTilde("~/.openMSX");
#endif
	return OPENMSX_DIR;
}

const string& FileOperations::getUserDataDir()
{
	static const string USER_DATA_DIR = getUserOpenMSXDir() + "/share";
	return USER_DATA_DIR;
}

const string& FileOperations::getSystemDataDir()
{
	static string systemDir;
	if (systemDir.empty()) {
#if	defined(__WIN32__)
		char p[MAX_PATH + 1];
		int res = GetModuleFileNameA(NULL, p, MAX_PATH);
		if ((res == 0) || (res == MAX_PATH)) {
			throw FatalError("Cannot detect openMSX directory.");
		}
		if (!strrchr(p, '\\')) {
			throw FatalError("openMSX is not in directory!?");
		}
		*(strrchr(p, '\\')) = '\0';
		systemDir = getConventionalPath(p) + "/share";
#else
		systemDir = DATADIR; // defined in config.h (default /opt/openMSX/share)
#endif
	}
	return systemDir;
}

} // namespace openmsx
