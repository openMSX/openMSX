// $Id$

#ifndef __ROMTYPES_HH__
#define __ROMTYPES_HH__

#include "openmsx.hh"
#include "MSXException.hh"


enum MapperType {
	GENERIC_8KB  = 0,
	GENERIC_16KB = 1,
	KONAMI5      = 2,
	KONAMI4      = 3,
	ASCII_8KB    = 4,
	ASCII_16KB   = 5,
	R_TYPE       = 6,
	PLAIN        = 15,

	HAS_SRAM     = 16,
	XANADU       = 16,
	ROYAL_BLOOD  = 17,
	HYDLIDE2     = 18,
	GAME_MASTER2 = 19,

	HAS_DAC      = 32,
	MAJUTSUSHI   = 32,
	SYNTHESIZER  = 33,
};

class NotInDataBaseException : public MSXException {
	public:
		NotInDataBaseException(const std::string &desc)
			: MSXException(desc) {}
};

class RomTypes
{
	public:
		static MapperType nameToMapperType(const std::string &name);
		static MapperType guessMapperType(byte* data, int size);
		static MapperType searchDataBase (byte* data, int size);
};

#endif
