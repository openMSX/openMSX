// $Id$

#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include "FileOperations.hh"
#include "openmsx.hh"
#ifdef	__WIN32__
#define WIN32_LEAN_AND_MEAN
#define	_WIN32_IE	0x0400
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#include <ctype.h>
#include <algorithm>
#define	MAXPATHLEN	MAX_PATH
#endif


namespace openmsx {

string FileOperations::expandTilde(const string &path)
{
	if ((path.size() <= 1) || (path[0] != '~')) {
		return path;
	}
	if (path[1] == '/') {
		// current user
		return getUserDir() + path.substr(1);
	} else {
		// other user
		PRT_INFO("Warning: ~<user>/ not yet implemented");
		return path;
	}
}


bool FileOperations::mkdirp(const string &path_)
{
	string path = expandTilde(path_);
	
	unsigned pos = path.find_first_of('/');
	while (pos != string::npos) {
		if (doMkdir(getNativePath(path).substr(0, pos + 1).c_str(), 0777) &&
		    (errno != EEXIST)) {
			return false;
		}
		pos = path.find_first_of('/', pos + 1);
	}
	if (doMkdir(getNativePath(path).c_str(), 0777) && (errno != EEXIST)) {
		return false;
	}
	
	return true;
}

string FileOperations::getFilename(const string &path)
{
	unsigned pos = path.rfind('/');
	return path.substr(pos + 1);
}

string FileOperations::getBaseName(const string &path)
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

int	FileOperations::doMkdir(const char *name, mode_t mode)
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

const string& FileOperations::getUserDir()
{
	static string userDir;
	if (userDir.empty()) {
#if	defined(__WIN32__)
		char p[MAX_PATH + 1];
		int res = SHGetSpecialFolderPathA(0, p, CSIDL_PERSONAL, 1);
		if (res != TRUE) {
			PRT_ERROR("Cannot get user directory.");
		}
		userDir = getConventionalPath(p);
#else
		userDir = getenv("HOME");
#endif
	}
	return userDir;
}

const string& FileOperations::getSystemDir()
{
	static string systemDir;
	if (systemDir.empty()) {
#if	defined(__WIN32__)
		char p[MAX_PATH + 1];
		int res = GetModuleFileNameA(NULL, p, MAX_PATH);
		if ((res == 0) || (res == MAX_PATH)) {
			PRT_ERROR("Cannot detect openMSX directory.");
		}
		if (!strrchr(p,'\\')) {
			PRT_ERROR("openMSX is not in directory!?");
		}
		*(strrchr(p,'\\')) = '\0';
		systemDir = getConventionalPath(p) + "/";
#else
		systemDir = "/opt/openMSX/";
#endif
	}
	return systemDir;
}

} // namespace openmsx
