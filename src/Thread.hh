// $Id$

#ifndef __THREAD_HH__
#define __THREAD_HH__

#include <SDL/SDL_thread.h>


class Runnable
{
	public:
		virtual void run() = 0;
};

class Thread
{
	public:
		Thread(Runnable *runnable);
		void start();
		void stop();

	private:
		static int startThread(void *runnable);

		Runnable *runnable;
		SDL_Thread *thread;
};
#endif
