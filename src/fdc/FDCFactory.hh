// $Id$

#ifndef __FDCFACTORY_HH__
#define __FDCFACTORY_HH__

namespace openmsx {

class EmuTime;
class MSXDevice;
class Config;

class FDCFactory
{
public:
	static MSXDevice* create(Config* config, const EmuTime& time);
};

} // namespace openmsx
#endif
