// $Id$

#ifndef __ROMFACTORY_HH__
#define __ROMFACTORY_HH__

#include "RomTypes.hh"
#include "MSXRom.hh"

class EmuTime;
class Device;


class RomFactory
{
	public:
		static MSXRom* create(Device* config, const EmuTime &time);

	private:
		static MapperType retrieveMapperType(Device *config,
		                              const EmuTime &time);
};

#endif
