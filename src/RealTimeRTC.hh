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
		// Must be a power of two, <= 8192.
		static const int RTC_HERTZ = 1024;
		
		RealTimeRTC();
		bool init();
		int readRTC();
		int nonBlockReadRTC();
		void reset(const EmuTime &time);
		
		int rtcFd;
		bool resyncFlag;
		int overslept;
		int prevOverslept;
		EmuTimeFreq<RTC_HERTZ> emuRef;
};

#endif
