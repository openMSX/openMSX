// $Id$

#ifndef ROMFACTORY_HH
#define ROMFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class XMLElement;

namespace RomFactory
{
	std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const XMLElement& config);
}

} // namespace openmsx

#endif
