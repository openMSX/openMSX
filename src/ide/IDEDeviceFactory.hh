// $Id$

#ifndef __IDEDEVICEFACTORY_HH__
#define __IDEDEVICEFACTORY_HH__

#include <string>

class IDEDevice;
class EmuTime;

using namespace std;


class IDEDeviceFactory
{
	public:
		static IDEDevice* create(const string &name,
		                         const EmuTime &time);
};

#endif
