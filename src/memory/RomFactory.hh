// $Id$

#ifndef __ROMFACTORY_HH__
#define __ROMFACTORY_HH__

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
