// $Id: 

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"


class MSXRealTime : public MSXDevice
{
	public:
		~MSXRealTime(); 
		static MSXRealTime *instance();
		
		void reset();
		void executeUntilEmuTime(const Emutime &time);

		float getRealDuration(Emutime time1, Emutime time2);

	private:
		static const int SYNCINTERVAL    = 50;	// sync every 50ms
		static const int MAX_CATCHUPTIME = 2000;	// max nb of ms overtime
		static const int MIN_REALTIME    = 40;	// min duration of interval
	
		MSXRealTime(); 
		static MSXRealTime *oneInstance;

		Emutime emuRef, emuOrigin;
		unsigned int realRef, realOrigin;	// !! Overflow in 49 days
		int catchUpTime;  // number of milliseconds overtime.
		float factor;
		
		// tune exponential average (0 < ALPHA < 1)
		//  ALPHA small -> past is more important
		//        BIG   -> present is more important
		static const float ALPHA = 0.5;
};
#endif
