// $Id$

#ifdef	__WIN32__
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
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "build-info.hh"
#include "FileOperations.hh"
#include "openmsx.hh"
#include "CliCommOutput.hh"
#include "MSXException.hh"


#if	defined(__WIN32__)
#ifdef	__cplusplus
extern "C" {
#endif
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

#ifdef	__cplusplus
}
#endif
#endif


namespace openmsx {

/* A wrapper for mkdir().  On some systems, mkdir() does not take permision in
 * arguments. For such systems, in this function, adjust arguments.
 */
static int doMkdir(const char* name, mode_t mode)
{
#if	defined(__MINGW32__) || defined(_MSC_VER)
	if ((name[0]=='/' || name[0]=='\\') && name[1]=='\0' || 
	    (name[1]==':' && name[3]=='\0' && (name[2]=='/' || name[2]=='\\' || name[2]=='\0')
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


void FileOperations::mkdirp(const string& path_)
{
	if (path_.empty()) {
		return;
	}
	string path = expandTilde(path_);

	string::size_type pos = 0;
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
	string::size_type pos = path.rfind('/');
	if (pos == string::npos) {
		return path;
	} else {
		return path.substr(pos + 1);
	}
}

string FileOperations::getBaseName(const string& path)
{
	string::size_type pos = path.rfind('/');
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

string FileOperations::getUserDataDir()
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

string FileOperations::getSystemDataDir()
{
	const char* const NAME = "OPENMSX_SYSTEM_DATA";
	char* value = getenv(NAME);
	if (value) {
		return value;
	}

	string newValue;
#if defined(__WIN32__)
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
#else
	newValue = DATADIR; // defined in build-info.h (default /opt/openMSX/share)
#endif
	setenv(NAME, newValue.c_str(), 0);
	return newValue;
}

string FileOperations::expandCurrentDirFromDrive (const string& path)
{			
	string result = path;
#ifdef __WIN32__
	if (((path.size() == 2) && (path[1]==':')) ||
		((path.size() >=3) && (path[1]==':') && (path[2] != '/'))){
		// get current directory for this drive
		unsigned char drive = tolower(path[0]);
		if (('a' <= drive) && (drive <='z')){
			char buffer [MAX_PATH + 1];
			if (_getdcwd(drive - 'a' +1, buffer, MAX_PATH) != NULL){
				result = buffer;
				result = getConventionalPath(result);
				result.append("/");
				result.append(path.substr(2));
			}
		}
	}
#endif
	return result;
}

bool FileOperations::isRegularFile(const string& filename)
{
	struct stat st;
	int ret = stat(filename.c_str(), &st);
	return (ret == 0) && S_ISREG(st.st_mode);
}

bool FileOperations::isDirectory(const string& directory)
{
	struct stat st;
	int ret = stat(directory.c_str(), &st);
	return (ret == 0) && S_ISDIR(st.st_mode);
}

bool FileOperations::exists(const string& filename)
{
	struct stat st;
	return stat(filename.c_str(), &st) == 0;
}

} // namespace openmsx
