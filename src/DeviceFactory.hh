// $Id$

#ifndef DEVICEFACTORY_HH
#define DEVICEFACTORY_HH

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
