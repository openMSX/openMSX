#ifndef SCHEDULABLE_HH
#define SCHEDULABLE_HH

#include "EmuTime.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"
#include <cassert>
#include <vector>

namespace openmsx {

class Scheduler;

// For backwards-compatible savestates
struct SyncPointBW
{
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/) {
		assert(Archive::IS_LOADER);
		ar.serialize("time", time,
		             "type", userData);
	}

	EmuTime time = EmuTime::zero();
	int userData = 0;
};

/**
 * Every class that wants to get scheduled at some point must inherit from
 * this class.
 */
class Schedulable
{
public:
	Schedulable(const Schedulable&) = delete;
	Schedulable& operator=(const Schedulable&) = delete;

	/**
	 * When the previously registered syncPoint is reached, this
	 * method gets called.
	 */
	virtual void executeUntil(EmuTime::param time) = 0;

	/**
	 * Just before the the Scheduler is deleted, it calls this method of
	 * all the Schedulables that are still registered.
	 * If you override this method you should unregister this Schedulable
	 * in the implementation. The default implementation just prints a
	 * diagnostic (and soon after the Scheduler will trigger an assert that
	 * there are still registered Schedulables.
	 * Normally there are easier ways to unregister a Schedulable. ATM this
	 * method is only used in AfterCommand (because it's not part of the
	 * MSX machine).
	 */
	virtual void schedulerDeleted();

	[[nodiscard]] Scheduler& getScheduler() const { return scheduler; }

	/** Convenience method:
	  * This is the same as getScheduler().getCurrentTime(). */
	[[nodiscard]] EmuTime::param getCurrentTime() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	template<typename Archive>
	static std::vector<SyncPointBW> serializeBW(Archive& ar) {
		assert(Archive::IS_LOADER);
		ar.beginTag("Schedulable");
		std::vector<SyncPointBW> result;
		ar.serialize("syncPoints", result);
		ar.endTag("Schedulable");
		return result;
	}
	template<typename Archive>
	static void restoreOld(Archive& ar, std::vector<Schedulable*> schedulables) {
		assert(Archive::IS_LOADER);
		for (auto* s : schedulables) {
			s->removeSyncPoints();
		}
		for (auto& old : serializeBW(ar)) {
			unsigned i = old.userData;
			if (i < schedulables.size()) {
				schedulables[i]->setSyncPoint(old.time);
			}
		}
	}

protected:
	explicit Schedulable(Scheduler& scheduler);
	~Schedulable();

	void setSyncPoint(EmuTime::param timestamp);
	bool removeSyncPoint();
	void removeSyncPoints();
	[[nodiscard]] bool pendingSyncPoint() const;
	[[nodiscard]] bool pendingSyncPoint(EmuTime& result) const;

private:
	Scheduler& scheduler;
};
REGISTER_BASE_CLASS(Schedulable, "Schedulable");

} // namespace openmsx

#endif
