// $Id: DummyCassetteDevice.hh,v

#ifndef __DUMMYCASSETTEDEVICE_HH__
#define __DUMMYCASSETTEDEVICE_HH__

#include "CassetteDevice.hh"

class DummyCassetteDevice : public CassetteDevice
{
	public:
		DummyCassetteDevice();
		static DummyCassetteDevice* instance();
	
		void setMotor(bool status, const Emutime &time);
		short readSample(const Emutime &time);
		void writeWave(short *buf, int length);
		int getWriteSampleRate();

	private:
		static DummyCassetteDevice *oneInstance;
};
#endif
