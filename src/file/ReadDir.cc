// $Id$

#include "ReadDir.hh"

namespace openmsx {

ReadDir::ReadDir(const std::string& directory)
{
	dir = opendir(directory.c_str());
}

ReadDir::~ReadDir()
{
	if (dir) {
		closedir(dir);
	}
}

struct dirent* ReadDir::getEntry()
{
	if (!dir) {
		return NULL;
	}
	return readdir(dir);
}

} // namespace openmsx
