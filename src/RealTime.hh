#ifndef MSXREALTIME_HH
#define MSXREALTIME_HH

#include "EmuTime.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "Schedulable.hh"

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
	[[nodiscard]] double getRealDuration(EmuTime::param time1, EmuTime::param time2) const;

	/** Convert RealTime to EmuTime.
	  */
	[[nodiscard]] EmuDuration getEmuDuration(double realDur) const;

	/** Check that there is enough real time left before we reach as certain
	  * point in emulated time.
	  * @param us Real time duration is micro seconds.
	  * @param time Point in emulated time.
	  */
	[[nodiscard]] bool timeLeft(uint64_t us, EmuTime::param time) const;

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
	bool signalEvent(const Event& event) override;

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
	EmuTime emuTime = EmuTime::zero();
	double sleepAdjust;
	bool enabled = true;
};

} // namespace openmsx

#endif
