// $Id$

#include "ReadDir.hh"

using std::string;

namespace openmsx {

ReadDir::ReadDir(const string& directory)
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
