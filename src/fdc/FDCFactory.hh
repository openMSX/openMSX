// $Id$

#ifndef __FDCFACTORY_HH__
#define __FDCFACTORY_HH__

#include <memory>

using std::auto_ptr;

namespace openmsx {

class EmuTime;
class MSXDevice;
class Config;

class FDCFactory
{
public:
	static auto_ptr<MSXDevice> create(Config* config, const EmuTime& time);
};

} // namespace openmsx
#endif
