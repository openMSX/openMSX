// $Id$

#ifndef ROMFACTORY_HH
#define ROMFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class EmuTime;
class XMLElement;

class RomFactory
{
public:
	static std::auto_ptr<MSXDevice> create(const XMLElement& config,
	                                       const EmuTime& time);
};

} // namespace openmsx

#endif
