// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"

// forward declarations
class EmuTime;


class DummyDevice : public MSXIODevice, public MSXMemDevice
{
	public:
		~DummyDevice();
		static DummyDevice *instance();
		void init();
		void reset(const EmuTime &time);
	
	private:
		DummyDevice(MSXConfig::Device *config, const EmuTime &time);
};

#endif //__DUMMYDEVICE_HH__

