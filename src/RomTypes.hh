// $Id$

#ifndef __ROMTYPES_HH__
#define __ROMTYPES_HH__

#include <string>
#include "openmsx.hh"
#include "MSXException.hh"


class NotInDataBaseException : public MSXException {
	public:
		NotInDataBaseException(const std::string &desc)
			: MSXException(desc) {}
};

class RomTypes
{
	public:
		static int nameToMapperType(const std::string &name);
		static int guessMapperType(byte* data, int size);
		static int searchDataBase (byte* data, int size);
};

#endif
