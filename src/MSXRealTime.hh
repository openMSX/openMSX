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
		static const int SYNCINTERVAL = 50;	// sync every 50ms
	
		MSXRealTime(); 
		static MSXRealTime *oneInstance;

		Emutime emuRef;
		unsigned int realRef;
		int catchUpTime;  // number of milliseconds overtime.
		static const int MAX_CATCHUPTIME = 5000;  // number of milliseconds overtime.
		float factor;
		static const float ALPHA = 0.5;
};
#endif
