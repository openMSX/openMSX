// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

#include <memory>

using std::auto_ptr;

namespace openmsx {

class MSXDevice;
class EmuTime;

class DeviceFactory {
public:
	static auto_ptr<MSXDevice> create(Config* conf, const EmuTime& time);
};

} // namespace openmsx

#endif
