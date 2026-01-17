#ifndef READDIR_HH
#define READDIR_HH

#include "direntp.hh"
#include "zstring_view.hh"

#include <memory>
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
	explicit ReadDir(zstring_view directory)
		: dir(opendir(directory.empty() ? "." : directory.c_str())) {}

	/** Get directory entry for next file. Returns nullptr when there
	  * are no more entries or in case of error (e.g. given directory
	  * does not exist).
	  */
	[[nodiscard]] struct dirent* getEntry() {
		if (!dir) return nullptr;
		return readdir(dir.get());
	}

	/** Is the given directory valid (does it exist)?
	  */
	[[nodiscard]] bool isValid() const { return dir != nullptr; }

private:
	struct CloseDir {
		static void operator()(DIR* d) { if (d) closedir(d); }
	};
	using DIR_t = std::unique_ptr<DIR, CloseDir>;
	DIR_t dir;
};

} // namespace openmsx

#endif
