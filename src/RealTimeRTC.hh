// $Id$

#ifndef __MSXREALTIMERTC_HH__
#define __MSXREALTIMERTC_HH__

#include "RealTime.hh"


class RealTimeRTC : public RealTime
{
	public:
		static RealTimeRTC* create();
		virtual ~RealTimeRTC(); 

	protected:
		virtual float doSync(const EmuTime &time);  
		virtual void resync();
		
	private:
		RealTimeRTC();
		void reset(const EmuTime &time);
		
		int rtcFd;
		bool initOK;
		bool resyncFlag;
		EmuTimeFreq<8192> emuRef;
};

#endif
