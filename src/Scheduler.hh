// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include <vector>
#include "EmuTime.hh"
#include "Settings.hh"
#include "Semaphore.hh"

using std::vector;

namespace openmsx {

class MSXCPU;
class Schedulable;
class Renderer;

class Scheduler : private SettingListener
{
private:
	class SynchronizationPoint
	{
	public:
		SynchronizationPoint(const EmuTime& time,
				     Schedulable* dev, int usrdat)
			: timeStamp(time), device(dev), userData(usrdat) {}
		const EmuTime& getTime() const { return timeStamp; }
		Schedulable* getDevice() const { return device; }
		int getUserData() const { return userData; }
		bool operator<(const SynchronizationPoint& n) const
			{ return getTime() > n.getTime(); } // smaller time is higher priority
	private:
		EmuTime timeStamp;
		Schedulable* device;
		int userData;
	};

public:
	static Scheduler* instance();

	/**
	 * Register a syncPoint. When the emulation reaches "timestamp",
	 * the executeUntil() method of "device" gets called.
	 * SyncPoints are ordered: smaller EmuTime -> scheduled
	 * earlier.
	 * The supplied EmuTime may not be smaller than the current CPU
	 * time.
	 * If you want to schedule something as soon as possible, you
	 * can pass Scheduler::ASAP as time argument.
	 * A device may register several syncPoints.
	 * Optionally a "userData" parameter can be passed, this
	 * parameter is not used by the Scheduler but it is passed to
	 * the executeUntil() method of "device". This is useful
	 * if you want to distinguish between several syncPoint types.
	 * If you do not supply "userData" it is assumed to be zero.
	 */
	void setSyncPoint(const EmuTime& timestamp, Schedulable* device,
	                  int userData = 0);

	/**
	 * Removes a syncPoint of a given device that matches the given
	 * userData.
	 * If there is more than one match only one will be removed,
	 * there is no guarantee that the earliest syncPoint is
	 * removed.
	 * Removing a syncPoint is a relatively expensive operation,
	 * if possible don't remove the syncPoint but ignore it in
	 * your executeUntil() method.
	 */
	void removeSyncPoint(Schedulable* device, int userdata = 0);

	/**
	 * Schedule till a certain moment in time.
	 * It's alllowed to call this method recursivly.
	 */
	void schedule(const EmuTime& limit);

	/**
	 * This stops the schedule loop, should only be used by the
	 * quit program routine.
	 */
	void stopScheduling();

	/** Set renderer to call when emulation is paused.
	  * TODO: Function will be moved to OSD later.
	  */
	void setRenderer(Renderer* renderer) {
		this->renderer = renderer;
	}

	void powerOn();
	void powerOff();

	// Should only be called by VDP 
	void pause();
	bool isPaused() const {
		return instance()->pauseSetting.getValue();
	}

	static const EmuTime ASAP;

private:
	Scheduler();
	virtual ~Scheduler();

	// SettingListener
	virtual void update(const SettingLeafNode* setting);

	void unpause();

	/** Vector used as heap, not a priority queue because that
	  * doesn't allow removal of non-top element.
	  */
	vector<SynchronizationPoint> syncPoints;
	Semaphore sem;	// protects syncPoints

	/** Should the emulation continue running?
	  */
	bool emulationRunning;

	bool paused;
	MSXCPU* cpu;
	Renderer* renderer;
	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
};

} // namespace openmsx

#endif
