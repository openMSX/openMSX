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
		Thread(Runnable *runnable);
		~Thread();
		void start();
		void stop();

	private:
		static int startThread(void *runnable);

		Runnable *runnable;
		SDL_Thread *thread;
};
#endif
