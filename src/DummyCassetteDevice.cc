// $Id: DummyCassetteDevice.cc,v

#include "DummyCassetteDevice.hh"

DummyCassetteDevice::DummyCassetteDevice()
{
}

DummyCassetteDevice *DummyCassetteDevice::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new DummyCassetteDevice();
	}
	return oneInstance;
}
DummyCassetteDevice *DummyCassetteDevice::oneInstance = NULL;

void DummyCassetteDevice::setMotor(bool status, const Emutime &time)
{
	// do nothing
}

short DummyCassetteDevice::readSample(const Emutime &time)
{
	return 32767;	// TODO check value
}

void DummyCassetteDevice::writeWave(short *buf, int length)
{
	// do nothing
}
int DummyCassetteDevice::getWriteSampleRate()
{
	return 0;	// 0 means not interested
}
