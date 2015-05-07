#ifndef PRECACHEFILE_HH
#define PRECACHEFILE_HH

#include "Thread.hh"
#include <atomic>
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

	std::atomic<bool> exitLoop;
};

} // namespace openmsx

#endif
