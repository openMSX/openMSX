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
	 * Get the real time (in ms)
	 */
	virtual unsigned getTime() = 0;

	/**
	 * Convert EmuTime to RealTime and vice versa
	 */
	float getRealDuration(const EmuTime& time1, const EmuTime& time2);
	EmuDuration getEmuDuration(float realDur);

	/**
	 * Synchronize EmuTime with RealTime, normally this is done
	 * automatically, but some devices have additional information
	 * and can indicate 'good' moments to sync, eg: VDP can call
	 * this method at the end of each frame.
	 */
	float sync(const EmuTime& time);

private:
	Scheduler& scheduler;
	SettingsConfig& settingsConfig;

protected:
	RealTime(); 
	void initBase();
	virtual void doSleep(unsigned ms) = 0;
	virtual void reset() = 0;
	
private:
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;

	// SettingListener
	void update(const SettingLeafNode* setting) throw();

	float internalSync(const EmuTime& time);
	float doSync(const EmuTime& time);  
	void resync();
	void reset(const EmuTime &time);

	IntegerSetting speedSetting;
	int maxCatchUpTime;	// max nb of ms overtime
	int maxCatchUpFactor;	// max catch up speed factor (percentage)

	BooleanSetting throttleSetting;
	BooleanSetting& pauseSetting;
	BooleanSetting& powerSetting;

	bool resyncFlag;
	unsigned int realRef, realOrigin;	// !! Overflow in 49 days
	EmuTimeFreq<1000> emuRef, emuOrigin;	// in ms (rounding err!!)
	int catchUpTime;  // number of milliseconds overtime
	float emuFactor;
	float sleepAdjust;
};

} // namespace openmsx

#endif // __MSXREALTIME_HH__
