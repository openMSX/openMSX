// $Id$

#ifndef DEVICEFACTORY_HH
#define DEVICEFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class XMLElement;
class EmuTime;

class DeviceFactory {
public:
	static std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const XMLElement& conf,
		const EmuTime& time);
};

} // namespace openmsx

#endif
