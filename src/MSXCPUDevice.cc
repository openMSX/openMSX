
#include "MSXCPUDevice.hh"

MSXCPUDevice::MSXCPUDevice(int freq) : currentTime(freq, 0)
{
}

MSXCPUDevice::~MSXCPUDevice()
{
}

Emutime &MSXCPUDevice::getCurrentTime()
{
	return currentTime;
}
