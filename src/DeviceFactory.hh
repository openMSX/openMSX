// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

#include <memory>

namespace openmsx {

class MSXDevice;
class XMLElement;
class EmuTime;

class DeviceFactory {
public:
	static std::auto_ptr<MSXDevice> create(const XMLElement& conf,
	                                       const EmuTime& time);
};

} // namespace openmsx

#endif
