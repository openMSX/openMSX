// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "Schedulable.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "SettingListener.hh"
#include "EmuTime.hh"

namespace openmsx {

class Scheduler;
class SettingsConfig;

class RealTime : private Schedulable, private SettingListener
{
public:
	static RealTime& instance();

	/** Convert EmuTime to RealTime.
	  */
	float getRealDuration(const EmuTime& time1, const EmuTime& time2);
	
	/** Convert RealTime to EmuTime.
	  */
	EmuDuration getEmuDuration(float realDur);

	/** Check that there is enough real time left before we reach as certain
	  * point in emulated time.
	  * @param us Real time duration is micro seconds.
	  * @param time Point in emulated time.
	  */
	bool timeLeft(unsigned long long us, const EmuTime& time);
	
	/** Synchronize EmuTime with RealTime.
	  * @param time The current emulation time.
	  * @param allowSleep Is this method allowed to sleep, typically the
	  *                   result of a previous call to timeLeft() is passed.
	  */
	void sync(const EmuTime& time, bool allowSleep);

private:
	RealTime(); 
	virtual ~RealTime();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	// SettingListener
	void update(const SettingLeafNode* setting);

	void internalSync(const EmuTime& time, bool allowSleep);
	void resync();

	Scheduler& scheduler;
	SettingsConfig& settingsConfig;

	IntegerSetting speedSetting;

	BooleanSetting throttleSetting;
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;

	unsigned long long idealRealTime;
	EmuTime emuTime;
	float sleepAdjust;
};

} // namespace openmsx

#endif // __MSXREALTIME_HH__
