// $Id$

#include "RealTimeRTC.hh"

#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// Must be a power of two, <= 8192.
const int RTC_HERTZ = 1024;


RealTimeRTC* RealTimeRTC::create()
{
	RealTimeRTC* rt = new RealTimeRTC();
	if (!rt->initOK) {
		delete rt;
		rt = NULL;
	}
	return rt;
}


RealTimeRTC::RealTimeRTC()
{
	initOK = false;
	
	// Open RTC device.
	rtcFd = open("/dev/rtc", O_RDONLY);
	if (rtcFd == -1) {
		PRT_INFO("Couldn't open /dev/rtc");
		return;
	}

	// Select 1024Hz.
	int retval = ioctl(rtcFd, RTC_IRQP_SET, RTC_HERTZ);
	if (retval == -1) {
		PRT_INFO("Couldn't select " << RTC_HERTZ << "Hz timer");
		return;
	}

	// Enable periodic interrupts.
	retval = ioctl(rtcFd, RTC_PIE_ON, 0);
	if (retval == -1) {
		PRT_INFO("Couldn't enable timer interrupts");
		return;
	}
	
	initOK = true;

	reset(EmuTime::zero);
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

void RealTimeRTC::resync()
{
	resyncFlag = true;
}

float RealTimeRTC::doSync(const EmuTime &curEmu)
{
	if (resyncFlag) {
		reset(curEmu);
		return 1.0;
	}
	
	int speed = 25600 / speedSetting.getValue();
	int emuPassed8K = (int)((speed * emuRef.getTicksTill(curEmu)) >> 8);
	int emuPassedRTC = (emuPassed8K * RTC_HERTZ) / 8192;

	//fprintf(stderr, "syncing %d ticks: ", emuPassedRTC);
	while (emuPassedRTC > 0) {
		// Blocks until interrupt.
		unsigned long data;
		int retval = read(rtcFd, &data, sizeof(unsigned long));
		if (retval == -1) {
			perror("read");
			exit(errno);
		}
		int nrInts = data >> 8;
		//fprintf(stderr, " %d",nrInts);
		emuPassedRTC -= nrInts;
	}
	//fprintf(stderr, "\n");
	if (emuPassedRTC >= 0) {
		//fprintf(stderr, "ok\n");
	} else {
		fprintf(stderr, "%d ", -emuPassedRTC);
	}

	emuRef = curEmu;
	return 1.0;
}

void RealTimeRTC::reset(const EmuTime &time)
{
	resyncFlag = false;
	unsigned long data;
	int retval = read(rtcFd, &data, sizeof(unsigned long));
	if (retval == -1) {
		perror("read");
		exit(errno);
	}
	emuRef = time;
}
