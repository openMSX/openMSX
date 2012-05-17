// $Id$

#ifndef ROMFACTORY_HH
#define ROMFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class DeviceConfig;

namespace RomFactory
{
	std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const DeviceConfig& config);
}

} // namespace openmsx

#endif
