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
#define	MAXPATHLEN	MAX_PATH
#endif


namespace openmsx {

extern	string systemdir;
string usrdir;

string FileOperations::expandTilde(const string &path)
{
	if (path.size() <= 1) {
		return path;
	}
	if (path[0] != '~') {
		return path;
	}
	if (path[1] == '/') {
		// current user
		return usrdir + path.substr(1);
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
		string empty;
		return empty;
	} else {
		return path.substr(0, pos + 1);
	}
}

string FileOperations::getNativePath(const string &path)
{
	string	ret = path;
#if	defined(__WIN32__)
	unsigned int	i;

	for (i=0;i<ret.length();i++)
		if (ret[i]=='/')
			ret[i] = '\\';
#endif
	return	ret;
}

string FileOperations::getConventionalPath(const string &path)
{
	string	ret = path;
#if	defined(__WIN32__)
	unsigned int	i;

	for (i=0;i<ret.length();i++)
		if (ret[i]=='\\')
			ret[i] = '/';
#endif
	return	ret;
}

int	FileOperations::doMkdir(const char *name, mode_t mode)
{
#if	defined(__MINGW32__) || defined(_MSC_VER)
	if ((name[0]=='/' || name[0]=='\\') && name[1]=='\0' || 
	    (name[1]==':' && name[3]=='\0' && (name[2]=='/' || name[2]=='\\')
	    && ((name[0]>='A' && name[0]<='Z')||(name[0]>='a' && name[0]<='z'))
	    )) {
/*		*(_errno())=EEXIST;
		return	-1;
*/		return	0;
	} else {
		return	mkdir(name);
	}
#else
	return	mkdir(name, mode);
#endif
}

int	FileOperations::isAbsolutePath(string path)
{
#if	defined(__WIN32__)
	if (path[0]=='/' || (path[1]==':' && path[2]=='/' &&
	    ((path[0]>='A' && path[0]<='Z')||(path[0]>='a' && path[0]<='z'))))
#else
	if (path[0]=='/')
#endif
		return	1;
	else
		return	0;
}

int	FileOperations::setUsrDir(void)
{
#if	defined(__WIN32__)
	char	p[MAX_PATH+1];
	int	res;

	res = SHGetSpecialFolderPathA(0, p, CSIDL_PERSONAL, 1);
	if (res==TRUE) {
		usrdir = getConventionalPath(p);
		return	0;
	} else {
		usrdir = "";
		return	-1;
	}
#else
	usrdir = getenv("HOME");
	return	0;
#endif
}

int	FileOperations::setSysDir(void)
{
#if	defined(__WIN32__)
	char	p[MAX_PATH+1];
	int	res;

	res=GetModuleFileNameA(NULL,p,MAX_PATH);
	if (res==0 || res==MAX_PATH) {
		systemdir = "";
		return	-1;
	} else {
		systemdir = getConventionalPath(p) + "/";
		return	0;
	}
#else
//	systemdir = "/opt/openMSX/";
	return	0;
#endif
 }

} // namespace openmsx
