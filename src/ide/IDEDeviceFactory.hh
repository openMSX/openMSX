// $Id$

#ifndef __IDEDEVICEFACTORY_HH__
#define __IDEDEVICEFACTORY_HH__

#include <string>

class IDEDevice;
class EmuTime;


class IDEDeviceFactory
{
	public:
		static IDEDevice* create(const std::string &name,
		                         const EmuTime &time);
};

#endif
