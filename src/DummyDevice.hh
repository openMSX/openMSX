
#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include "MSXDevice.hh"
#include "emutime.hh"

class DummyDevice : public MSXDevice
{
	public:
		DummyDevice();
		~DummyDevice();
		static DummyDevice *instance();
		//static MSXDevice* instantiate();
		void setConfigDevice(MSXConfig::Device *config);
		void init();
		void start();
		void stop();
		void reset();
		void saveState(ofstream &writestream);
		void restoreState(string &devicestring, ifstream &readstream);
	private:
		static DummyDevice *oneInstance;
};

#endif //__DUMMYDEVICE_H__

