// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "Schedulable.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "Settings.hh"

class MSXCPU;
class Scheduler;


class RealTime : public Schedulable
{
	public:
		/**
		 * Destructor
		 */
		virtual ~RealTime(); 

		/**
		 * This is a singleton class. This method returns a reference
		 * to the single instance of this class.
		 */
		static RealTime *instance();
		
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		virtual const std::string &schedName() const;

		float getRealDuration(const EmuTime &time1, const EmuTime &time2);

		/**
		 * Synchronize EmuTime with RealTime, normally this is done
		 * automatically, but some devices have additional information
		 * and can indicate 'good' moments to sync, eg: VDP can call
		 * this method at the end of each frame.
		 */
		float sync(const EmuTime &time);

		/**
		 * Reset internal counters.
		 */
		void reset(const EmuTime &time);
		
	private:
		RealTime(); 
		void internalSync(const EmuTime &time);

		class PauseSetting : public BooleanSetting
		{
			public:
				PauseSetting();
				virtual bool checkUpdate(bool newValue,
				                         const EmuTime &time);
		} pauseSetting;
		
		class SpeedSetting : public IntegerSetting
		{
			public:
				SpeedSetting();
				virtual bool checkUpdate(int newValue,
				                         const EmuTime &time);
		} speedSetting;

		BooleanSetting throttleSetting;

		int syncInterval;	// sync every ..ms
		int maxCatchUpTime;	// max nb of ms overtime
		int maxCatchUpFactor;	// max catch up speed factor (percentage)

		// tune exponential average (0 < alpha < 1)
		//  alpha small -> past is more important
		//        big   -> present is more important
		static const float alpha = 0.5;	// TODO make tuneable???
	
		EmuTimeFreq<1000> emuRef, emuOrigin;	// in ms (rounding err!!)
		unsigned int realRef, realOrigin;	// !! Overflow in 49 days
		int catchUpTime;  // number of milliseconds overtime.
		float emuFactor;
		float totalFactor;
		float sleepAdjust;

		Scheduler *scheduler;
};

#endif
