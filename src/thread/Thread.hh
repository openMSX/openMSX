// $Id$

#ifndef __THREAD_HH__
#define __THREAD_HH__

// forward declarations
class SDL_Thread;


class Runnable
{
	public:
		virtual void run() = 0;
};

class Thread
{
	public:
		/** Create a new thread.
		  * @param runnable Object those run() method will be invoked by
		  * 	the created thread when it starts running.
		  */
		Thread(Runnable *runnable);
		
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
		  * It is not allowed to call this method on a stopped thread.
		  */
		void join();

	private:
		/** Helper function to start a thread (SDL is plain C).
		  */
		static int startThread(void *runnable);

		Runnable *runnable;
		SDL_Thread *thread;
};

#endif // __THREAD_HH__
