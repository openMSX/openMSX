// $Id: DummyCassetteDevice.cc,v

#include "DummyCassetteDevice.hh"

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
void DummyCassetteDevice::readWave(short *buf, int length, const Emutime &time)
{
	while(length--) {
		*(buf++) = 32767;	// TODO check value
	}
}
int DummyCassetteDevice::getReadSampleRate()
{
	return 44100;
}
void DummyCassetteDevice::writeWave(short *buf, int length, const Emutime &time)
{
	// do nothing
}
int DummyCassetteDevice::getWriteSampleRate()
{
	return 0;
}
