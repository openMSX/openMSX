// $Id$

#ifndef __CONDVAR_HH__
#define __CONDVAR_HH__

#include <SDL/SDL.h>


class CondVar
{
	public:
		CondVar();
		~CondVar();
		void wait();
		void signal();
		void signalAll();

	private:
		SDL_cond* cond;
		SDL_mutex* mutex;
};
#endif
