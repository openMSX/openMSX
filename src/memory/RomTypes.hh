// $Id$

#ifndef __ROMTYPES_HH__
#define __ROMTYPES_HH__

namespace openmsx {

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
	KOREAN80IN1  = 11,
	KOREAN90IN1  = 12,
	KOREAN126IN1 = 13,
	HOLY_QURAN   = 14,
	PLAIN        = 15,
	FSA1FM1      = 16,
	FSA1FM2      = 17,

	PAGE0        = 32 + 1,
	PAGE1        = 32 + 2,
	PAGE01       = 32 + 3,
	PAGE2        = 32 + 4,
	PAGE02       = 32 + 5,
	PAGE12       = 32 + 6,
	PAGE012      = 32 + 7,
	PAGE3        = 32 + 8,
	PAGE03       = 32 + 9,
	PAGE13       = 32 + 10,
	PAGE013      = 32 + 11,
	PAGE23       = 32 + 12,
	PAGE023      = 32 + 13,
	PAGE123      = 32 + 14,
	PAGE0123     = 32 + 15,
	
	HAS_SRAM     = 64,
	ASCII8_8     = 64 + 0,
	HYDLIDE2     = 64 + 1,
	GAME_MASTER2 = 64 + 2,
	PANASONIC    = 64 + 3,
	NATIONAL     = 64 + 4,
	KOEI_8       = 64 + 5,
	KOEI_32      = 64 + 6,

	HAS_DAC      = 128,
	MAJUTSUSHI   = 128 + 0,
	SYNTHESIZER  = 128 + 1,

	UNKNOWN      = 256
};

} // namespace openmsx

#endif
