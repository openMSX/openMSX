// $Id$

#ifndef __ROMFACTORY_HH__
#define __ROMFACTORY_HH__

#include <memory>

using std::auto_ptr;

namespace openmsx {

class MSXDevice;
class EmuTime;
class XMLElement;

class RomFactory
{
public:
	static auto_ptr<MSXDevice> create(const XMLElement& config,
	                                  const EmuTime& time);
};

} // namespace openmsx

#endif
