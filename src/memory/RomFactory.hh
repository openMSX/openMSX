// $Id$

#ifndef __ROMFACTORY_HH__
#define __ROMFACTORY_HH__

namespace openmsx {

class MSXRom;
class EmuTime;
class Device;

class RomFactory
{
public:
	static MSXRom* create(Device* config, const EmuTime& time);
};

} // namespace openmsx

#endif
