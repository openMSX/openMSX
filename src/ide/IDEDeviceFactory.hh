// $Id$

#ifndef __IDEDEVICEFACTORY_HH__
#define __IDEDEVICEFACTORY_HH__

#include <string>

namespace openmsx {

class IDEDevice;
class EmuTime;

using std::string;


class IDEDeviceFactory
{
	public:
		static IDEDevice* create(const string &name,
		                         const EmuTime &time);
};

} // namespace openmsx

#endif
