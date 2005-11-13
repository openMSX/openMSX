// $Id$

#ifndef SEMAPHORE_HH
#define SEMAPHORE_HH

#include "noncopyable.hh"
#include <SDL.h>

namespace openmsx {

class Semaphore : private noncopyable
{
public:
	explicit Semaphore(unsigned value);
	~Semaphore();
	void up();
	void down();

private:
	SDL_sem* semaphore;
};

class ScopedLock : private noncopyable
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
