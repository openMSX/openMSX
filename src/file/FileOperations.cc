// $Id$

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "FileOperations.hh"
#include "openmsx.hh"


std::string FileOperations::expandTilde(const std::string &path)
{
	if (path[0] != '~') {
		return path;
	}

	if (path[1] == '/') {
		// current user
		return std::string(getenv("HOME")) + path.substr(1);
	} else {
		// other user
		PRT_ERROR("Error: ~<user>/ not yet implemented");
		return path;
	}
}


bool FileOperations::mkdirp(const std::string &path_)
{
	std::string path = expandTilde(path_);
	
	unsigned pos = path.find_first_of('/');
	while (pos != std::string::npos) {
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

std::string FileOperations::getFilename(const std::string &path)
{
	unsigned pos = path.rfind('/');
	return path.substr(pos + 1);
}

std::string FileOperations::getBaseName(const std::string &path)
{
	unsigned pos = path.rfind('/');
	return path.substr(0, pos);
}
