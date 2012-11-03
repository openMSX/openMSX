// $Id$

#ifndef SEMAPHORE_HH
#define SEMAPHORE_HH

#include "noncopyable.hh"
#include <SDL.h>
#include <cassert>

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
	ScopedLock()
		: lock(nullptr)
	{
	}

	explicit ScopedLock(Semaphore& lock_)
		: lock(nullptr)
	{
		take(lock_);
	}

	~ScopedLock()
	{
		release();
	}

	void take(Semaphore& lock_)
	{
		assert(!lock);
		lock = &lock_;
		lock->down();
	}

	void release()
	{
		if (lock) {
			lock->up();
			lock = nullptr;
		}
	}

private:
	Semaphore* lock;
};

} // namespace openmsx

#endif
