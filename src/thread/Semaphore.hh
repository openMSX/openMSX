// $Id$

#ifndef __SEMAPHORE_HH__
#define __SEMAPHORE_HH__

#include <SDL/SDL.h>


class Semaphore
{
	public:
		Semaphore(unsigned value);
		~Semaphore();
		void up();
		void down();

	private:
		SDL_sem* semaphore;
};

#endif
