// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

#include <memory>

using std::auto_ptr;

namespace openmsx {

class MSXDevice;
class XMLElement;
class EmuTime;

class DeviceFactory {
public:
	static auto_ptr<MSXDevice> create(const XMLElement& conf, const EmuTime& time);
};

} // namespace openmsx

#endif
