// $Id$

#ifndef __MSXREALTIME_HH__
#define __MSXREALTIME_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"


class MSXRealTime : public MSXDevice, public HotKeyListener
{
	public:
		/**
		 * Destructor
		 */
		~MSXRealTime(); 

		/**
		 * This is a singleton class. This method returns a reference
		 * to the single instance of this class.
		 */
		static MSXRealTime *instance();
		
		void reset(const EmuTime &time);
		void executeUntilEmuTime(const EmuTime &time);

		float getRealDuration(EmuTime time1, EmuTime time2);

		void signalHotKey(SDLKey key);

	private:
		/**
		 * Constructor.
		 */
		MSXRealTime(MSXConfig::Device *config, const EmuTime &time); 

		//TODO put these in config file
		static const int SYNCINTERVAL    = 50;	// sync every 50ms
		static const int MAX_CATCHUPTIME = 1500;	// max nb of ms overtime
		static const int MIN_REALTIME    = 40;	// min duration of interval
	
		static MSXRealTime *oneInstance;

		EmuTimeFreq<1000> emuRef, emuOrigin;	// in ms (rounding err!!)
		unsigned int realRef, realOrigin;	// !! Overflow in 49 days
		int catchUpTime;  // number of milliseconds overtime.
		float factor;
		
		// tune exponential average (0 < ALPHA < 1)
		//  ALPHA small -> past is more important
		//        BIG   -> present is more important
		static const float ALPHA = 0.5;

		bool paused;
};
#endif
