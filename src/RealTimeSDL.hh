// $Id$

#ifndef __MSXREALTIMESDL_HH__
#define __MSXREALTIMESDL_HH__

#include "RealTime.hh"


namespace openmsx {

class RealTimeSDL : public RealTime
{
	public:
		RealTimeSDL();
		virtual ~RealTimeSDL();

	protected:
		virtual float doSync(const EmuTime &time);
		virtual void resync();

	private:
		void reset(const EmuTime &time);

		bool resyncFlag;
		unsigned int realRef, realOrigin;	// !! Overflow in 49 days
		EmuTimeFreq<1000> emuRef, emuOrigin;	// in ms (rounding err!!)
		int catchUpTime;  // number of milliseconds overtime
		float emuFactor;
		float sleepAdjust;
};

} // namespace openmsx

#endif
