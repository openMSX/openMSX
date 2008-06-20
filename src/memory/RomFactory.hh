// $Id$

#ifndef ROMFACTORY_HH
#define ROMFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class XMLElement;

class RomFactory
{
public:
	static std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const XMLElement& config);
};

} // namespace openmsx

#endif
