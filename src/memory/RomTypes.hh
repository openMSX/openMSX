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
	CROSS_BLAIM  = 7,
	MSX_AUDIO    = 8,
	HARRY_FOX    = 9,
	HALNOTE      = 10,
	PLAIN        = 15,

	PAGE0        = 16 + 1,
	PAGE1        = 16 + 2,
	PAGE01       = 16 + 3,
	PAGE2        = 16 + 4,
	PAGE02       = 16 + 5,
	PAGE12       = 16 + 6,
	PAGE012      = 16 + 7,
	PAGE3        = 16 + 8,
	PAGE03       = 16 + 9,
	PAGE13       = 16 + 10,
	PAGE013      = 16 + 11,
	PAGE23       = 16 + 12,
	PAGE023      = 16 + 13,
	PAGE123      = 16 + 14,
	PAGE0123     = 16 + 15,
	
	HAS_SRAM     = 32,
	ASCII8_8     = 32 + 0,
	HYDLIDE2     = 32 + 1,
	GAME_MASTER2 = 32 + 2,
	PANASONIC    = 32 + 3,
	NATIONAL     = 32 + 4,

	HAS_DAC      = 64,
	MAJUTSUSHI   = 64 + 0,
	SYNTHESIZER  = 64 + 1,

	UNKNOWN      = 128
};

class RomTypes
{
	public:
		static MapperType nameToMapperType(const std::string &name);
		static MapperType guessMapperType(const byte* data, int size);
		static MapperType searchDataBase (const byte* data, int size);
};

#endif
