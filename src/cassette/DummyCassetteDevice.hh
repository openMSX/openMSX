// $Id$

#ifndef __DUMMYCASSETTEDEVICE_HH__
#define __DUMMYCASSETTEDEVICE_HH__

#include "CassetteDevice.hh"


namespace openmsx {

class DummyCassetteDevice : public CassetteDevice
{
	public:
		DummyCassetteDevice();

		virtual void setMotor(bool status, const EmuTime &time);
		virtual short readSample(const EmuTime &time);
		virtual void writeWave(short *buf, int length);
		virtual int getWriteSampleRate();

		virtual void plug(Connector* connector, const EmuTime& time) throw();
		virtual void unplug(const EmuTime& time);
};

} // namespace openmsx
#endif
