// $Id$

#ifndef SEMAPHORE_HH
#define SEMAPHORE_HH

#include <SDL.h>

namespace openmsx {

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

class ScopedLock
{
public:
	explicit ScopedLock(Semaphore& lock_)
		: lock(lock_)
	{
		lock.down();
	}

	~ScopedLock()
	{
		lock.up();
	}

private:
	Semaphore& lock;
};

} // namespace openmsx

#endif
