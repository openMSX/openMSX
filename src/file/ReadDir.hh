// $Id$

#ifndef READDIR_HH
#define READDIR_HH

#include <string>
#include <sys/types.h>
#include <dirent.h>

namespace openmsx {

/**
 * Simple wrapper around openmdir() / readdir() / closedir() functions.
 * Mainly usefull to automatically call closedir() when object goes out
 * of scope.
 */
class ReadDir
{
public:
	ReadDir(const std::string& directory);
	~ReadDir();

	struct dirent* getEntry();

private:
	DIR* dir;
};

} // namespace openmsx

#endif
