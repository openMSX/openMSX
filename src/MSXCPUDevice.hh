// $Id$

#ifndef __MSXCPUDEVICE_HH__
#define __MSXCPUDEVICE_HH__

#include "emutime.hh"

class MSXCPUDevice
{
	friend class MSXCPU;

	public:
		MSXCPUDevice(int freq);
		virtual ~MSXCPUDevice();
		virtual void init() = 0;
		virtual void reset() = 0;
		virtual void IRQ(bool irq) = 0;
		virtual void executeUntilTarget() = 0;
		virtual Emutime &getCurrentTime();
	
	protected:
		Emutime currentTime;
};

#endif //__MSXCPUDEVICE_HH__
