// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include <fstream>
#include <string>
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
		void saveState(std::ofstream &writestream);
		void restoreState(std::string &devicestring, std::ifstream &readstream);
	private:
		DummyDevice(MSXConfig::Device *config, const EmuTime &time);
		static DummyDevice *oneInstance;
};

#endif //__DUMMYDEVICE_HH__

