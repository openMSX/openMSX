// $Id$

#ifndef __MUTEX_HH__
#define __MUTEX_HH__

struct SDL_mutex;


namespace openmsx {

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

} // namespace openmsx

#endif
