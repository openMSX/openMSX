#ifndef MSXREALTIME_HH
#define MSXREALTIME_HH

#include "Schedulable.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include <cstdint>

namespace openmsx {

class MSXMotherBoard;
class GlobalSettings;
class EventDistributor;
class EventDelay;
class BooleanSetting;
class SpeedManager;
class ThrottleManager;
class Setting;

class RealTime final : private Schedulable, private EventListener
                     , private Observer<Setting>
                     , private Observer<SpeedManager>
                     , private Observer<ThrottleManager>
{
public:
	explicit RealTime(
		MSXMotherBoard& motherBoard, GlobalSettings& globalSettings,
		EventDelay& eventDelay);
	~RealTime();

	/** Convert EmuTime to RealTime.
	  */
	double getRealDuration(EmuTime::param time1, EmuTime::param time2);

	/** Convert RealTime to EmuTime.
	  */
	EmuDuration getEmuDuration(double realDur);

	/** Check that there is enough real time left before we reach as certain
	  * point in emulated time.
	  * @param us Real time duration is micro seconds.
	  * @param time Point in emulated time.
	  */
	bool timeLeft(uint64_t us, EmuTime::param time);

	void resync();

	void enable();
	void disable();

private:
	/** Synchronize EmuTime with RealTime.
	  * @param time The current emulation time.
	  * @param allowSleep Is this method allowed to sleep, typically the
	  *                   result of a previous call to timeLeft() is passed.
	  */
	void sync(EmuTime::param time, bool allowSleep);

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) noexcept override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;
	// Observer<SpeedManager>
	void update(const SpeedManager& speedManager) noexcept override;
	// Observer<ThrottleManager>
	void update(const ThrottleManager& throttleManager) noexcept override;

	void internalSync(EmuTime::param time, bool allowSleep);

	MSXMotherBoard& motherBoard;
	EventDistributor& eventDistributor;
	EventDelay& eventDelay;
	SpeedManager& speedManager;
	ThrottleManager& throttleManager;
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;

	uint64_t idealRealTime;
	EmuTime emuTime;
	double sleepAdjust;
	bool enabled;
};

} // namespace openmsx

#endif
