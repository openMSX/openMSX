// $Id$

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "FileOperations.hh"
#include "openmsx.hh"


namespace openmsx {

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
		return string(getenv("HOME")) + path.substr(1);
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
		if (mkdir(path.substr(0, pos + 1).c_str(), 0777) &&
		    (errno != EEXIST)) {
			return false;
		}
		pos = path.find_first_of('/', pos + 1);
	}
	if (mkdir(path.c_str(), 0777) && (errno != EEXIST)) {
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

} // namespace openmsx
