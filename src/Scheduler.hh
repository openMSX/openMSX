// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include <vector>
#include "EmuTime.hh"
#include "BooleanSetting.hh"
#include "SettingListener.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "Schedulable.hh"

using std::vector;


namespace openmsx {

class Leds;
class MSXCPU;
class CommandController;
class Renderer;
class MSXMotherBoard;
class InputEventGenerator;
class EventDistributor;

class Scheduler : private SettingListener, private EventListener, private Schedulable
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
	static Scheduler& instance();

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
	 * Like setSyncPoint, but safe to call from another thread.
	 * Always schedules ASAP.
	 */
	void setAsyncPoint(Schedulable* device, int userData = 0);

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
	 * Get the current scheduler time.
	 */
	const EmuTime& getCurrentTime() const;

	/**
	 * Set scheduler time. Only CPU is allowed to call this method
	 */
	void setCurrentTime(const EmuTime& time);
	
	/**
	 * Schedule till a certain moment in time.
	 */
	void schedule();
	void scheduleFromCPU(const EmuTime& limit);

	/** Set renderer to call when emulation is paused.
	  * TODO: Function will be moved to OSD later.
	  */
	void setRenderer(Renderer* renderer);
	void setMotherBoard(MSXMotherBoard* motherboard);
	void setEventDistributor(EventDistributor* eventDistributor);
	void unsetEventDistributor(EventDistributor* eventDistributor);
	
	void powerOn();
	void powerOff();

	void increasePauseCounter() { ++pauseCounter; }
	void decreasePauseCounter() { --pauseCounter; }

	BooleanSetting& getPauseSetting() {
		return pauseSetting;
	}
	BooleanSetting& getPowerSetting() {
		return powerSetting;
	}

	static const EmuTime ASAP;

private:
	Scheduler();
	virtual ~Scheduler();

	/** Emulate a device.
	  * @param sp The sync point that requested this emulation.
	  * @param time The time until which the device will be emulated.
	  * 	This is an actual time stamp (ASAP is already resolved).
	  */
	void scheduleDevice(const SynchronizationPoint &sp, const EmuTime &time);

	// SettingListener
	virtual void update(const SettingLeafNode* setting);

	// EventListener
	virtual bool signalEvent(const Event& event);

	void pause();
	void unpause();
	void stopScheduling();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;
	
	/** Vector used as heap, not a priority queue because that
	  * doesn't allow removal of non-top element.
	  */
	vector<SynchronizationPoint> syncPoints;
	Semaphore sem;	// protects syncPoints

	/** Should the emulation continue running?
	  */
	bool emulationRunning;
	int pauseCounter;

	bool paused;
	bool powered;
	bool needReset;
	EmuTime scheduleTime;

	Renderer* renderer;
	MSXMotherBoard* motherboard;
	InputEventGenerator* eventGenerator;
	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	
	Leds& leds;
	MSXCPU& cpu;
	CommandController& commandController;

	class QuitCommand : public SimpleCommand {
	public:
		QuitCommand(Scheduler& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		Scheduler& parent;
	} quitCommand;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(Scheduler& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		Scheduler& parent;
	} resetCommand;
};

} // namespace openmsx

#endif
