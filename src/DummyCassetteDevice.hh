// $Id$

#ifndef __DUMMYCASSETTEDEVICE_HH__
#define __DUMMYCASSETTEDEVICE_HH__

#include "CassetteDevice.hh"

class DummyCassetteDevice : public CassetteDevice
{
	public:
		DummyCassetteDevice();
		static DummyCassetteDevice* instance();
	
		void setMotor(bool status, const EmuTime &time);
		short readSample(const EmuTime &time);
		void writeWave(short *buf, int length);
		int getWriteSampleRate();

	private:
		static DummyCassetteDevice *oneInstance;
};
#endif
