// $Id$

#include "config.h"
#ifdef HAVE_LINUX_RTC_H

#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cerrno>
#include <sstream>
#include "RealTimeRTC.hh"
#include "CliCommOutput.hh"

using std::ostringstream;

namespace openmsx {

RealTimeRTC* RealTimeRTC::create()
{
	RealTimeRTC* rt = new RealTimeRTC();
	if (!rt->init()) {
		delete rt;
		CliCommOutput::instance().printWarning(
			"Couldn't initialize RTC timer, falling back to (less accurate) SDL timer...");
		rt = NULL;
	}
	return rt;
}


RealTimeRTC::RealTimeRTC()
{
}

RealTimeRTC::~RealTimeRTC()
{
	if (rtcFd == -1) {
		return;
	}
	// Disable periodic interrupts.
	int retval = ioctl(rtcFd, RTC_PIE_OFF, 0);
	if (retval == -1) {
		// ignore error
	}
	close(rtcFd);
}

bool RealTimeRTC::init()
{
	// Open RTC device.
	rtcFd = open("/dev/rtc", O_RDONLY);
	if (rtcFd == -1) {
		CliCommOutput::instance().printWarning(
			"RTC timer: couldn't open /dev/rtc");
		return false;
	}

	// Select 1024Hz.
	int retval = ioctl(rtcFd, RTC_IRQP_SET, RTC_HERTZ);
	if (retval == -1) {
		ostringstream out;
		out << "RTC timer: couldn't select " << RTC_HERTZ << "Hz timer; try adding\n"
		    << "   echo " << RTC_HERTZ << " > /proc/sys/dev/rtc/max-user-freq\n"
		    << "to your system startup scripts.";
		CliCommOutput::instance().printWarning(out.str());
		return false;
	}

	// Enable periodic interrupts.
	retval = ioctl(rtcFd, RTC_PIE_ON, 0);
	if (retval == -1) {
		CliCommOutput::instance().printWarning(
			"RTC timer: couldn't enable timer interrupts");
		return false;
	}
	
	initBase();
	return true;
}


unsigned RealTimeRTC::getTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;	// ms
}

void RealTimeRTC::doSleep(unsigned ms)
{
	unsigned slept = 0;
	while (slept < ms) {
		// Blocks until interrupt
		unsigned long data;
		int retval = read(rtcFd, &data, sizeof(unsigned long));
		if (retval == -1) {
			throw FatalError(strerror(errno));
		}
		slept += (data >> 8);	// TODO 1024 is not ms
	}
}

void RealTimeRTC::reset()
{
	unsigned long data;
	int retval = read(rtcFd, &data, sizeof(unsigned long));
	if (retval == -1) {
		throw FatalError(strerror(errno));
	}
}

} // namespace openmsx

#endif // HAVE_LINUX_RTC_H
