// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"


class RealTime : public Schedulable, public HotKeyListener
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

		void signalHotKey(SDLKey key);

	private:
		/**
		 * Constructor.
		 */
		RealTime(); 

		int syncInterval;	// sync every ..ms
		int maxCatchUpTime;	// max nb of ms overtime
		int minRealTime;	// min duration of interval

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

		static const int SCHEDULE_SELF = 1;	// != 0
};
#endif
