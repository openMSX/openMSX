// $Id$

#ifndef MUTEX_HH
#define MUTEX_HH

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
