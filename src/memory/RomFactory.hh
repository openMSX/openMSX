// $Id$

#ifndef __ROMFACTORY_HH__
#define __ROMFACTORY_HH__

namespace openmsx {

class MSXRom;
class EmuTime;
class Config;

class RomFactory
{
public:
	static MSXRom* create(Config* config, const EmuTime& time);
};

} // namespace openmsx

#endif
