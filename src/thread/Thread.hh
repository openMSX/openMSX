#ifndef THREAD_HH
#define THREAD_HH

namespace openmsx::Thread {

	// For debugging only
	/** Store ID of the main thread, should be called exactly once from
	  * the main thread.
	  */
	void setMainThread();

	/** Returns true when called from the main thread.
	  */
	[[nodiscard]] bool isMainThread();

} // namespace openmsx::Thread

#endif
