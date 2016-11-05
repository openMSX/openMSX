#ifndef THREAD_HH
#define THREAD_HH

#include <memory>

namespace std {
	class thread;
};

namespace openmsx {

class Runnable
{
public:
	virtual void run() = 0;

protected:
	~Runnable() {}
};

class Thread
{
public:
	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	/** Create a new thread.
	  * @param runnable Object those run() method will be invoked by
	  *                 the created thread when it starts running.
	  */
	explicit Thread(Runnable* runnable);

	~Thread();

	/** Start this thread.
	  * It is not allowed to call this method on a running thread.
	  */
	void start();

	/** Waits for this thread to terminate.
	  * This method must be called on a started thread before it can be
	  * destructed.
	  */
	void join();

	// For debugging only
	/** Store ID of the main thread, should be called exactly once from
	  * the main thread.
	  */
	static void setMainThread();

	/** Returns true when called from the main thread.
	  */
	static bool isMainThread();

private:
	/** Helper function to start a thread.
	  */
	static int startThread(Runnable* runnable);

	Runnable* runnable;
	std::unique_ptr<std::thread> thread;
};

} // namespace openmsx

#endif
