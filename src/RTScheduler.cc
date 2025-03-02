#include "RTScheduler.hh"

#include "RTSchedulable.hh"

#include <algorithm>
#include <limits>

namespace openmsx {

struct EqualRTSchedulable {
	explicit EqualRTSchedulable(const RTSchedulable& schedulable_)
		: schedulable(schedulable_) {}
	bool operator()(const RTSyncPoint& sp) const {
		return sp.schedulable == &schedulable;
	}
	const RTSchedulable& schedulable;
};

void RTScheduler::add(uint64_t delta, RTSchedulable& schedulable)
{
	queue.insert(RTSyncPoint{.time = Timer::getTime() + delta, .schedulable = &schedulable},
	             [](RTSyncPoint& sp) {
	                     sp.time = std::numeric_limits<uint64_t>::max(); },
	             [](const RTSyncPoint& x, const RTSyncPoint& y) {
	                     return x.time < y.time; });
}

bool RTScheduler::remove(RTSchedulable& schedulable)
{
	return queue.remove(EqualRTSchedulable(schedulable));
}

bool RTScheduler::isPending(const RTSchedulable& schedulable) const
{
	return std::ranges::any_of(queue, EqualRTSchedulable(schedulable));
}

void RTScheduler::scheduleHelper(uint64_t limit)
{
	// Process at most this many events to prevent getting stuck in an
	// infinite loop when a RTSchedulable keeps on rescheduling itself in
	// the (too) near future.
	auto count = queue.size();
	while (true) {
		auto* schedulable = queue.front().schedulable;
		queue.remove_front();

		schedulable->executeRT();

		// It's possible RTSchedulables are canceled in the mean time,
		// so we can't rely on 'count' to replace this empty check.
		if (queue.empty()) break;
		if (queue.front().time > limit) [[likely]] break;
		if (--count == 0) break;
	}
}

} // namespace openmsx
