#ifndef THREAD_HH
#define THREAD_HH

struct SDL_Thread;

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

	/** Destroys this thread.
	  * Only use this method as a last resort, because it kills the thread
	  * with no regard for locks or resources the thread may be holding,
	  * I/O it is performing etc.
	  * If this method is called on a stopped thread, nothing happens.
	  */
	void stop();

	/** Waits for this thread to terminate.
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
	/** Helper function to start a thread (SDL is plain C).
	  */
	static int startThread(void* runnable);

	Runnable* runnable;
	SDL_Thread* thread;
};

} // namespace openmsx

#endif
