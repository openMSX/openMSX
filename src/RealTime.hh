// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "Scheduler.hh"
#include "EmuTime.hh"

// forward declarations
class MSXCPU;


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
		
		void executeUntilEmuTime(const EmuTime &time, int userData);

		float getRealDuration(const EmuTime time1, const EmuTime time2);

		/**
		 * Synchronize EmuTime with RealTime, normally this is done
		 * automatically, but some devices have additional information
		 * and can indicate 'good' moments to sync, eg: VDP can call
		 * this method at the end of each frame.
		 */
		void sync();

	private:
		void internalSync(const EmuTime &time);
		
		/**
		 * Constructor.
		 */
		RealTime(); 

		int syncInterval;	// sync every ..ms
		int maxCatchUpTime;	// max nb of ms overtime
		int maxCatchUpFactor;	// max catch up speed factor (percentage)

		// tune exponential average (0 < alpha < 1)
		//  alpha small -> past is more important
		//        big   -> present is more important
		static const float alpha = 0.5;	// TODO make tuneable???
	
		static RealTime *oneInstance;

		EmuTimeFreq<1000> emuRef, emuOrigin;	// in ms (rounding err!!)
		unsigned int realRef, realOrigin;	// !! Overflow in 49 days
		int catchUpTime;  // number of milliseconds overtime.
		float factor;

		bool paused;
		MSXCPU *cpu;
		Scheduler *scheduler;

		class PauseCmd : public ConsoleCommand {
			public:
				virtual void execute(const char *commandLine);
				virtual void help(const char *commandLine);
		};
		friend class PauseCmd;
		PauseCmd pauseCmd;
};
#endif
