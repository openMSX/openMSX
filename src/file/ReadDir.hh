// $Id$

/**
 * Simple wrapper around openmdir() / readdir() / closedir() functions.
 * Mainly usefull to automatically call closedir() when object goes out
 * of scope.
 */

#ifndef __READDIR_HH__
#define __READDIR_HH__

#include <string>
#include <sys/types.h>
#include <dirent.h>

namespace openmsx {

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
