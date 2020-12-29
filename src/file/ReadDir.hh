#ifndef READDIR_HH
#define READDIR_HH

#include "direntp.hh"
#include "zstring_view.hh"
#include <sys/types.h>

namespace openmsx {

/**
 * Simple wrapper around opendir() / readdir() / closedir() functions.
 * Mainly useful to automatically call closedir() when object goes out
 * of scope.
 */
class ReadDir
{
public:
	ReadDir(const ReadDir&) = delete;
	ReadDir& operator=(const ReadDir&) = delete;

	explicit ReadDir(zstring_view directory);
	~ReadDir();

	/** Get directory entry for next file. Returns nullptr when there
	  * are no more entries or in case of error (e.g. given directory
	  * does not exist).
	  */
	[[nodiscard]] struct dirent* getEntry();

	/** Is the given directory valid (does it exist)?
	  */
	[[nodiscard]] bool isValid() const { return dir != nullptr; }

private:
	DIR* dir;
};

} // namespace openmsx

#endif
