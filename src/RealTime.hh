// $Id$

#ifndef MSXREALTIME_HH
#define MSXREALTIME_HH

#include "Schedulable.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "EmuTime.hh"

namespace openmsx {

class Scheduler;
class EventDistributor;
class UserInputEventDistributor;
class GlobalSettings;
class IntegerSetting;
class BooleanSetting;
class ThrottleManager;
class Setting;

class RealTime : private Schedulable, private EventListener,
                 private Observer<Setting>, private Observer<ThrottleManager>
{
public:
	RealTime(Scheduler& scheduler, EventDistributor& eventDistributor,
	         UserInputEventDistributor& eventDistributor,
	         GlobalSettings& globalSettings);
	~RealTime();

	/** Convert EmuTime to RealTime.
	  */
	double getRealDuration(const EmuTime& time1, const EmuTime& time2);

	/** Convert RealTime to EmuTime.
	  */
	EmuDuration getEmuDuration(double realDur);

	/** Check that there is enough real time left before we reach as certain
	  * point in emulated time.
	  * @param us Real time duration is micro seconds.
	  * @param time Point in emulated time.
	  */
	bool timeLeft(unsigned long long us, const EmuTime& time);

private:
	/** Synchronize EmuTime with RealTime.
	  * @param time The current emulation time.
	  * @param allowSleep Is this method allowed to sleep, typically the
	  *                   result of a previous call to timeLeft() is passed.
	  */
	void sync(const EmuTime& time, bool allowSleep);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	// EventListener
	virtual void signalEvent(const Event& event);

	// Observer<Setting>
	void update(const Setting& setting);
	// Observer<ThrottleManager>
	void update(const ThrottleManager& throttleManager);

	void internalSync(const EmuTime& time, bool allowSleep);
	void resync();

	EventDistributor& eventDistributor;
	UserInputEventDistributor& userInputEventDistributor;
	ThrottleManager& throttleManager;
	IntegerSetting& speedSetting;
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;

	unsigned long long idealRealTime;
	EmuTime emuTime;
	double sleepAdjust;
};

} // namespace openmsx

#endif
