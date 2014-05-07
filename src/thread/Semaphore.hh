#ifndef SEMAPHORE_HH
#define SEMAPHORE_HH

#include "noncopyable.hh"
#include <cassert>
#include <condition_variable>
#include <mutex>

namespace openmsx {

class Semaphore : private noncopyable
{
public:
	explicit Semaphore(unsigned value);
	void up();
	void down();

private:
	std::mutex mutex;
	std::condition_variable condition;
	unsigned value;
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
