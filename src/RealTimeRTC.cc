// $Id$

#ifndef	NO_LINUX_RTC

#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
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
	
	reset(EmuTime::zero);

	return true;
}


void RealTimeRTC::resync()
{
	resyncFlag = true;
}

float RealTimeRTC::doSync(const EmuTime &time)
{
	if (resyncFlag) {
		reset(time);
		return 1.0;
	}
	
	int speed = 25600 / speedSetting.getValue();
	int normalizedEmuTicks = (speed * emuRef.getTicksTill(time)) >> 8;
	int sleep = normalizedEmuTicks - overslept;
	if ((sleep * maxCatchUpTime) < (normalizedEmuTicks * 100)) {
		// avoid catching up too fast
		sleep = (normalizedEmuTicks * 100) / maxCatchUpTime;
		overslept -= normalizedEmuTicks - sleep;
	} else {
		// we can catch up all lost time
		overslept = 0;
	}
	assert(sleep >= 0);

	int realTime = nonBlockReadRTC();
	sleep -= realTime;
	float factor = ((float)(realTime + prevOverslept)) /
	               ((float)(normalizedEmuTicks));
	
	while (sleep > 0) {
		sleep -= readRTC();
	}
	if (sleep < 0) {
		// overslept
		prevOverslept = -sleep;
		overslept += prevOverslept;
	}

	emuRef = time;
	//PRT_DEBUG("RTC: "<<factor);
	return factor;
}

int RealTimeRTC::nonBlockReadRTC()
{
	// TODO implement non-blocking read
	return readRTC();
}

int RealTimeRTC::readRTC()
{
	// Blocks until interrupt
	unsigned long data;
	int retval = read(rtcFd, &data, sizeof(unsigned long));
	if (retval == -1) {
		throw FatalError(strerror(errno));
	}
	return data >> 8;
}

void RealTimeRTC::reset(const EmuTime &time)
{
	resyncFlag = false;
	unsigned long data;
	int retval = read(rtcFd, &data, sizeof(unsigned long));
	if (retval == -1) {
		throw FatalError(strerror(errno));
	}
	overslept = 0;
	prevOverslept = 0;
	emuRef = time;
}

} // namespace openmsx

#endif	// !NO_LINUX_RTC
