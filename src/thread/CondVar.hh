// $Id$

#ifndef __CONDVAR_HH__
#define __CONDVAR_HH__

// forward declarations
class SDL_cond;
class SDL_mutex;


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
