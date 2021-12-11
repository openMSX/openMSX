#include "ReadDir.hh"

namespace openmsx {

ReadDir::ReadDir(zstring_view directory)
	: dir(opendir(directory.empty() ? "." : directory.c_str()))
{
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
		return nullptr;
	}
	return readdir(dir);
}

} // namespace openmsx
