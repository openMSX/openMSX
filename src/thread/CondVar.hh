// $Id$

#ifndef __CONDVAR_HH__
#define __CONDVAR_HH__

#include <SDL.h>


namespace openmsx {

class CondVar
{
	public:
		CondVar();
		~CondVar();
		void wait();
		int waitTimeout(Uint32);
		void signal();
		void signalAll();

	private:
		SDL_cond* cond;
		SDL_mutex* mutex;
};

} // namespace openmsx
#endif
