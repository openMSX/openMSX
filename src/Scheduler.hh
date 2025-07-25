#ifndef SCHEDULER_HH
#define SCHEDULER_HH

#include "EmuTime.hh"
#include "SchedulerQueue.hh"
#include <vector>

namespace openmsx {

class Schedulable;
class MSXCPU;

class SynchronizationPoint
{
public:
	SynchronizationPoint() = default;
	SynchronizationPoint(EmuTime time, Schedulable* dev)
		: timeStamp(time), device(dev) {}
	[[nodiscard]] EmuTime getTime() const { return timeStamp; }
	void setTime(EmuTime time) { timeStamp = time; }
	[[nodiscard]] Schedulable* getDevice() const { return device; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	EmuTime timeStamp = EmuTime::zero();
	Schedulable* device = nullptr;
};


class Scheduler
{
public:
	using SyncPoints = std::vector<SynchronizationPoint>;

	Scheduler() = default;
	~Scheduler();

	void setCPU(MSXCPU* cpu_)
	{
		cpu = cpu_;
	}

	/**
	 * Get the current scheduler time.
	 */
	[[nodiscard]] EmuTime getCurrentTime() const;

	/**
	 * TODO
	 */
	[[nodiscard]] EmuTime getNext() const
	{
		return queue.front().getTime();
	}

	/**
	 * Schedule till a certain moment in time.
	 */
	void schedule(EmuTime limit)
	{
		if (EmuTime next = getNext(); limit >= next) [[unlikely]] {
			scheduleHelper(limit, next); // slow path not inlined
		}
		scheduleTime = limit;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private: // -> intended for Schedulable
	friend class Schedulable;

	/**
	 * Register a syncPoint. When the emulation reaches "timestamp",
	 * the executeUntil() method of "device" gets called.
	 * SyncPoints are ordered: smaller EmuTime -> scheduled
	 * earlier.
	 * The supplied EmuTime may not be smaller than the current CPU
	 * time.
	 * A device may register several syncPoints.
	 */
	void setSyncPoint(EmuTime timestamp, Schedulable& device);

	[[nodiscard]] SyncPoints getSyncPoints(const Schedulable& device) const;

	/**
	 * Removes a syncPoint of a given device.
	 * If there is more than one match only one will be removed,
	 * there is no guarantee that the earliest syncPoint is
	 * removed.
	 * Returns false <=> if there was no match (so nothing removed)
	 */
	bool removeSyncPoint(const Schedulable& device);

	/** Remove all sync-points for the given device.
	  */
	void removeSyncPoints(const Schedulable& device);

	/**
	 * Is there a pending syncPoint for this device?
	 */
	[[nodiscard]] bool pendingSyncPoint(const Schedulable& device, EmuTime& result) const;

private:
	void scheduleHelper(EmuTime limit, EmuTime next);

private:
	/** Vector used as heap, not a priority queue because that
	  * doesn't allow removal of non-top element.
	  */
	SchedulerQueue<SynchronizationPoint> queue;
	EmuTime scheduleTime = EmuTime::zero();
	MSXCPU* cpu = nullptr;
	bool scheduleInProgress = false;
};

} // namespace openmsx

#endif
