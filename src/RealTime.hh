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
	virtual ~RealTime();

	static RealTime& instance();

	/**
	 * Get the real time (in us)
	 */
	virtual unsigned getRealTime() = 0;

	/**
	 * Convert EmuTime to RealTime and vice versa
	 */
	float getRealDuration(const EmuTime& time1, const EmuTime& time2);
	EmuDuration getEmuDuration(float realDur);

	
	bool timeLeft(unsigned us, const EmuTime& time);
	/**
	 * Synchronize EmuTime with RealTime, normally this is done
	 * automatically, but some devices have additional information
	 * and can indicate 'good' moments to sync, eg: VDP can call
	 * this method at the end of each frame.
	 */
	void sync(const EmuTime& time, bool allowSleep);

protected:
	RealTime(); 
	void initBase();
	virtual void doSleep(unsigned us) = 0;
	virtual void reset() = 0;
	
private:
	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;

	// SettingListener
	void update(const SettingLeafNode* setting) throw();

	void internalSync(const EmuTime& time, bool allowSleep);
	void resync();

	Scheduler& scheduler;
	SettingsConfig& settingsConfig;

	IntegerSetting speedSetting;

	BooleanSetting throttleSetting;
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;

	unsigned idealRealTime;
	EmuTime emuTime;
};

} // namespace openmsx

#endif // __MSXREALTIME_HH__
