
#ifndef __MSXCPUDEVICE_HH__
#define __MSXCPUDEVICE_HH__

#include "emutime.hh"

class MSXCPUDevice
{
	public:
		virtual void init() = 0;
		virtual void reset() = 0;
		virtual void IRQ(bool irq) = 0;
		virtual void executeUntilEmuTime(const Emutime &time) = 0;
};

#endif //__MSXCPUDEVICE_HH__
