// $Id$

#ifndef __MUTEX_HH__
#define __MUTEX_HH__

class SDL_mutex;


class Mutex
{
	public:
		Mutex();
		~Mutex();
		void grab();
		void release();

	private:
		SDL_mutex* mutex;
};
#endif
