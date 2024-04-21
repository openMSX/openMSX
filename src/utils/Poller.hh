#ifndef POLLER_HH
#define POLLER_HH

#include <array>
#include <atomic>

namespace openmsx {

/** Polls for events on a given file descriptor.
  * It is possible to abort this poll from another thread.
  * This class exists because in POSIX there is no straightforward way to
  * abort a blocking I/O operation.
  */
class Poller
{
public:
	Poller();
	Poller(const Poller&) = delete;
	Poller(Poller&&) = delete;
	Poller& operator=(const Poller&) = delete;
	Poller& operator=(Poller&&) = delete;
	~Poller();

#ifndef _WIN32
	/** Waits for an event to occur on the given file descriptor.
	  * Returns true iff abort() was called or an error occurred.
	  */
	[[nodiscard]] bool poll(int fd);
#endif

	/** Returns true iff abort() was called.
	  */
	[[nodiscard]] bool aborted() const {
		return abortFlag;
	}

	/** Aborts a poll in progress and any future poll attempts.
	  */
	void abort();

	/** Reset aborted() to false. (Functionally the same, but more efficient
	  * than destroying and recreating this object). */
	void reset() {
		abortFlag = false;
	}

private:
#ifndef _WIN32
	std::array<int, 2> wakeupPipe;
#endif
	std::atomic_bool abortFlag = false;
};

} // namespace openmsx

#endif
