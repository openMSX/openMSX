// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "Schedulable.hh"
#include "Settings.hh"
#include "EmuTime.hh"

namespace openmsx {

class RealTime : public Schedulable, private SettingListener
{
	public:
		virtual ~RealTime();
		static RealTime *instance();

		/** Does the user want to pause the emulator?
		  * @return true iff the pause setting is on.
		  */
		static bool isPaused() {
			return instance()->pauseSetting.getValue();
		}

		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		virtual const string &schedName() const;

		/**
		 * Convert EmuTime to RealTime and vice versa
		 */
		float getRealDuration(const EmuTime &time1, const EmuTime &time2);
		EmuDuration getEmuDuration(float realDur);

		/**
		 * Synchronize EmuTime with RealTime, normally this is done
		 * automatically, but some devices have additional information
		 * and can indicate 'good' moments to sync, eg: VDP can call
		 * this method at the end of each frame.
		 */
		float sync(const EmuTime &time);

	protected:
		RealTime(); 
		
		virtual float doSync(const EmuTime &time) = 0;  
		virtual void resync() = 0;

		IntegerSetting speedSetting;
		int maxCatchUpTime;	// max nb of ms overtime
		int maxCatchUpFactor;	// max catch up speed factor (percentage)

	private:
		float internalSync(const EmuTime &time);
		void update(const SettingLeafNode *setting);

		BooleanSetting pauseSetting;
		BooleanSetting throttleSetting;
};

} // namespace openmsx

#endif
