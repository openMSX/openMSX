#ifndef PRECACHEFILE_HH
#define PRECACHEFILE_HH

#include "Thread.hh"
#include "Semaphore.hh"
#include <string>

namespace openmsx {

/**
 * Read the complete file once and discard result. Hopefully the file
 * sticks in the OS cache. Mainly useful to avoid CDROM spinups or to
 * speed up real floppy disk (/dev/fd0) reads.
 */
class PreCacheFile final : private Runnable
{
public:
	explicit PreCacheFile(const std::string& name);
	~PreCacheFile();

private:
	// Runnable
	void run() override;

	const std::string name;
	Thread thread;

	Semaphore sem;
	bool exitLoop; // locked by sem
};

} // namespace openmsx

#endif
