// $Id$

#ifndef __DUMMYCASSETTEDEVICE_HH__
#define __DUMMYCASSETTEDEVICE_HH__

#include "CassetteDevice.hh"

// forward declarations
class EmuTime;


class DummyCassetteDevice : public CassetteDevice
{
	public:
		DummyCassetteDevice();
	
		virtual void setMotor(bool status, const EmuTime &time);
		virtual short readSample(const EmuTime &time);
		virtual void writeWave(short *buf, int length);
		virtual int getWriteSampleRate();

		virtual const std::string &getName();
};
#endif
