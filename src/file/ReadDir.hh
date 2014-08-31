#ifndef READDIR_HH
#define READDIR_HH

#include "noncopyable.hh"
#include "direntp.hh"
#include <string>
#include <sys/types.h>

namespace openmsx {

/**
 * Simple wrapper around openmdir() / readdir() / closedir() functions.
 * Mainly usefull to automatically call closedir() when object goes out
 * of scope.
 */
class ReadDir : private noncopyable
{
public:
	explicit ReadDir(const std::string& directory);
	~ReadDir();

	/** Get directory entry for next file. Returns nullptr when there
	  * are no more entries or in case of error (e.g. given directory
	  * does not exist).
	  */
	struct dirent* getEntry();

	/** Is the given directory valid (does it exist)?
	  */
	bool isValid() const { return dir != nullptr; }

private:
	DIR* dir;
};

} // namespace openmsx

#endif
