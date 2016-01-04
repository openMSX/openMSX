#ifndef ROMTYPES_HH
#define ROMTYPES_HH

namespace openmsx {

enum RomType {
	// Order doesn't matter (I sorted them alphabetically)
	ROM_ARC,
	ROM_ASCII8,
	ROM_ASCII8_2,
	ROM_ASCII8_8,
	ROM_ASCII16,
	ROM_ASCII16_2,
	ROM_ASCII16_8,
	ROM_CROSS_BLAIM,
	ROM_DOOLY,
	ROM_DRAM,
	ROM_FSA1FM1,
	ROM_FSA1FM2,
	ROM_GAME_MASTER2,
	ROM_GENERIC_8KB,
	ROM_GENERIC_16KB,
	ROM_HALNOTE,
	ROM_HAMARAJANIGHT,
	ROM_HARRY_FOX,
	ROM_HOLY_QURAN,
	ROM_HOLY_QURAN2,
	ROM_KBDMASTER,
	ROM_KOEI_8,
	ROM_KOEI_32,
	ROM_KONAMI,
	ROM_KONAMI_SCC,
	ROM_MAJUTSUSHI,
	ROM_MANBOW2,
	ROM_MANBOW2_2,
	ROM_MATRAINK,
	ROM_MEGAFLASHROMSCC,
	ROM_MEGAFLASHROMSCCPLUS,
	ROM_MEGAFLASHROMSCCPLUSSD,
	ROM_MIRRORED,
	ROM_MITSUBISHIMLTS2,
	ROM_MSXDOS2,
	ROM_MSXTRA,
	ROM_MSXWRITE,
	ROM_MULTIROM,
	ROM_NATIONAL,
	ROM_NETTOU_YAKYUU,
	ROM_NORMAL,
	ROM_PADIAL8,
	ROM_PADIAL16,
	ROM_PANASONIC,
	ROM_PLAYBALL,
	ROM_R_TYPE,
	ROM_SUPERLODERUNNER,
	ROM_SUPERSWANGI,
	ROM_SYNTHESIZER,
	ROM_WIZARDRY,
	ROM_ZEMINA80IN1,
	ROM_ZEMINA90IN1,
	ROM_ZEMINA126IN1,

	ROM_END_OF_UNORDERED_LIST, // not an actual romtype

	// For these the numeric value does matter
	ROM_PAGE0        = 128 + 1,  // value of lower 4 bits matters
	ROM_PAGE1        = 128 + 2,
	ROM_PAGE01       = 128 + 3,
	ROM_PAGE2        = 128 + 4,
	ROM_PAGE12       = 128 + 6,
	ROM_PAGE012      = 128 + 7,
	ROM_PAGE3        = 128 + 8,
	ROM_PAGE23       = 128 + 12,
	ROM_PAGE123      = 128 + 14,
	ROM_PAGE0123     = 128 + 15,
	ROM_MIRRORED0000 = 144 + 0, // value of lower 3 bits matters
	ROM_MIRRORED4000 = 144 + 2,
	ROM_MIRRORED8000 = 144 + 4,
	ROM_MIRROREDC000 = 144 + 6,
	ROM_NORMAL0000   = 152 + 0, // value of lower 3 bits matters
	ROM_NORMAL4000   = 152 + 2,
	ROM_NORMAL8000   = 152 + 4,
	ROM_NORMALC000   = 152 + 6,

	ROM_UNKNOWN      = 256,
	ROM_ALIAS        = 512, // no other enum value can have this bit set
};

// Make sure there is no overlap in numeric enum values between the unordered
// and ordered part of the enum list.
static_assert(int(ROM_END_OF_UNORDERED_LIST) < int(ROM_PAGE0), "renumber romtypes");

} // namespace openmsx

#endif
