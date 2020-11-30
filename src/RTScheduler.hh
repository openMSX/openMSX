#ifndef RTSCHEDULER_HH
#define RTSCHEDULER_HH

#include "SchedulerQueue.hh"
#include "Timer.hh"
#include "likely.hh"
#include <cstdint>

namespace openmsx {

class RTSchedulable;

struct RTSyncPoint
{
	uint64_t time;
	RTSchedulable* schedulable;
};

class RTScheduler
{
public:
	/** Execute all expired RTSchedulables. */
	inline void execute()
	{
		auto limit = Timer::getTime();
		if (!queue.empty() && unlikely(limit >= queue.front().time)) {
			scheduleHelper(limit); // slow path not inlined
		}
	}

private:
	// These are called by RTSchedulable
	friend class RTSchedulable;
	void add(uint64_t delta, RTSchedulable& schedulable);
	bool remove(RTSchedulable& schedulable);
	[[nodiscard]] bool isPending(const RTSchedulable& schedulable) const;

	void scheduleHelper(uint64_t limit);

private:
	SchedulerQueue<RTSyncPoint> queue;
};

} // namespace openmsx

#endif
