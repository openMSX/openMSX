// $Id$

#ifndef __REALTIMERTC_HH__
#define __REALTIMERTC_HH__

#include "config.h"
#ifdef HAVE_LINUX_RTC_H

#include "RealTime.hh"


namespace openmsx {

class RealTimeRTC : public RealTime
{
public:
	static RealTimeRTC* create();
	virtual ~RealTimeRTC();

protected:
	virtual unsigned getTime();
	virtual void doSleep(unsigned ms);
	virtual void reset();

private:
	// Must be a power of two, <= 8192.
	static const int RTC_HERTZ = 1024;

	RealTimeRTC();
	bool init();

	int rtcFd;
};

} // namespace openmsx

#endif // HAVE_LINUX_RTC_H

#endif // __REALTIMERTC_HH__
