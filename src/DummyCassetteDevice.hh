// $Id: DummyCassetteDevice.hh,v

#ifndef __DUMMYCASSETTEDEVICE_HH__
#define __DUMMYCASSETTEDEVICE_HH__

#include "CassetteDevice.hh"

class DummyCassetteDevice : public CassetteDevice
{
	public:
		static DummyCassetteDevice* instance();
	
		void setMotor(bool status, const Emutime &time);
		void readWave(short *buf, int length, const Emutime &time);
		int getReadSampleRate();
		void writeWave(short *buf, int length, const Emutime &time);
		int getWriteSampleRate();

	private:
		static DummyCassetteDevice *oneInstance;
};
#endif
