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

	private:
		static const int SYNCINTERVAL = 50;	// sync every 50ms
	
		MSXRealTime(); 
		static MSXRealTime *oneInstance;

		Emutime emuRef;
		unsigned int realRef;
};
#endif
