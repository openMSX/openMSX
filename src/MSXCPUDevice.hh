// $Id$

#ifndef __MSXCPUDEVICE_HH__
#define __MSXCPUDEVICE_HH__

#include "emutime.hh"

class MSXCPUDevice
{
	public:
		MSXCPUDevice(int freq);
		virtual ~MSXCPUDevice();
		virtual void init() = 0;
		virtual void reset() = 0;
		virtual void executeUntilTarget() = 0;
		virtual Emutime &getCurrentTime();
		virtual void setCurrentTime(Emutime &time);
	
	protected:
		Emutime currentTime;
};

#endif //__MSXCPUDEVICE_HH__
