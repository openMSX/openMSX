// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"


class DummyDevice : public MSXIODevice, public MSXMemDevice
{
	public:
		virtual ~DummyDevice();
		static DummyDevice *instance();
		virtual void reset(const EmuTime &time);
	
	private:
		DummyDevice(MSXConfig::Device *config, const EmuTime &time);
};

#endif //__DUMMYDEVICE_HH__

