// $Id$

#ifndef __MSXREALTIMERTC_HH__
#define __MSXREALTIMERTC_HH__

#include "RealTime.hh"


class RealTimeRTC : public RealTime
{
	public:
		static RealTimeRTC* RealTimeRTC::create();
		virtual ~RealTimeRTC(); 

	protected:
		virtual float doSync(const EmuTime &time);  
		virtual void resync();
		
	private:
		RealTimeRTC();
		
		int rtcFd;
		bool initOK;
		EmuTimeFreq<8192> emuRef;
};

#endif
