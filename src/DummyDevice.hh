// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include <fstream>
#include <string>

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "EmuTime.hh"

class DummyDevice : public MSXIODevice, public MSXMemDevice
{
	public:
		~DummyDevice();
		static DummyDevice *instance();
		void init();
		void start();
		void stop();
		void reset();
		void saveState(std::ofstream &writestream);
		void restoreState(std::string &devicestring, std::ifstream &readstream);
	private:
		DummyDevice();
		static DummyDevice *oneInstance;
};

#endif //__DUMMYDEVICE_HH__

