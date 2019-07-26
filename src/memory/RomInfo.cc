#include "RomInfo.hh"
#include "StringOp.hh"
#include "hash_map.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xxhash.hh"
#include <cassert>

using std::string;
using std::string_view;
using std::vector;

namespace openmsx {

static inline RomType makeAlias(RomType type)
{
	return static_cast<RomType>(ROM_ALIAS | type);
}
static inline RomType removeAlias(RomType type)
{
	return static_cast<RomType>(type & ~ROM_ALIAS);
}
static inline bool isAlias(RomType type)
{
	return (type & ROM_ALIAS) != 0;
}


static bool isInit = false;

// This maps a name to a RomType. There can be multiple names (aliases) for the
// same type.
using RomTypeMap = hash_map<string_view, RomType, XXHasher_IgnoreCase, StringOp::casecmp>;
static RomTypeMap romTypeMap(256); // initial hashtable size
                                   // (big enough so that no rehash is needed)

// This contains extra information for each RomType. This structure only
// contains the primary (non-alias) romtypes.
using RomTypeInfo = std::tuple<
	RomType,    // rom type
	string_view, // description
	unsigned>;  // blockSize
using RomTypeInfoMap = vector<RomTypeInfo>;
using RomTypeInfoMapLess = LessTupleElement<0>;
static RomTypeInfoMap romTypeInfoMap;

static void init(RomType type, string_view name,
                 unsigned blockSize, string_view description)
{
	romTypeMap.emplace_noCapacityCheck_noDuplicateCheck(name, type);
	romTypeInfoMap.emplace_back(type, description, blockSize);
}
static void initAlias(RomType type, string_view name)
{
	romTypeMap.emplace_noCapacityCheck_noDuplicateCheck(name, makeAlias(type));
}
static void init()
{
	if (isInit) return;
	isInit = true;

	// Generic ROM types that don't exist in real ROMs
	// (should not occur in any database!)
	init(ROM_GENERIC_8KB,    "8kB",            0x2000, "Generic 8kB");
	init(ROM_GENERIC_16KB,   "16kB",           0x4000, "Generic 16kB");

	// ROM mapper types for normal software (mainly games)
	init(ROM_KONAMI,         "Konami",         0x2000, "Konami MegaROM");
	init(ROM_KONAMI_SCC,     "KonamiSCC",      0x2000, "Konami with SCC");
	init(ROM_KBDMASTER,      "KeyboardMaster", 0x4000, "Konami Keyboard Master with VLM5030"); // officially plain 16K
	init(ROM_ASCII8,         "ASCII8",         0x2000, "ASCII 8kB");
	init(ROM_ASCII16,        "ASCII16",        0x4000, "ASCII 16kB");
	init(ROM_R_TYPE,         "R-Type",         0x4000, "R-Type");
	init(ROM_CROSS_BLAIM,    "CrossBlaim",     0x4000, "Cross Blaim");
	init(ROM_HARRY_FOX,      "HarryFox",       0x4000, "Harry Fox");
	init(ROM_HALNOTE,        "Halnote",        0x2000, "Halnote");
	init(ROM_ZEMINA80IN1,    "Zemina80in1",    0x2000, "Zemina 80 in 1");
	init(ROM_ZEMINA90IN1,    "Zemina90in1",    0x2000, "Zemina 90 in 1");
	init(ROM_ZEMINA126IN1,   "Zemina126in1",   0x2000, "Zemina 126 in 1");
	init(ROM_ASCII16_2,      "ASCII16SRAM2",   0x4000, "ASCII 16kB with 2kB SRAM");
	init(ROM_ASCII16_8,      "ASCII16SRAM8",   0x4000, "ASCII 16kB with 8kB SRAM");
	init(ROM_ASCII8_8,       "ASCII8SRAM8",    0x2000, "ASCII 8kB with 8kB SRAM");
	init(ROM_ASCII8_32,      "ASCII8SRAM32",   0x2000, "ASCII 8kB with 32kB SRAM");
	init(ROM_ASCII8_2,       "ASCII8SRAM2",    0x2000, "ASCII 8kB with 2kB SRAM");
	init(ROM_KOEI_8,         "KoeiSRAM8",      0x2000, "Koei with 8kB SRAM");
	init(ROM_KOEI_32,        "KoeiSRAM32",     0x2000, "Koei with 32kB SRAM");
	init(ROM_WIZARDRY,       "Wizardry",       0x2000, "Wizardry");
	init(ROM_GAME_MASTER2,   "GameMaster2",    0x1000, "Konami's Game Master 2");
	init(ROM_MAJUTSUSHI,     "Majutsushi",     0x2000, "Hai no Majutsushi");
	init(ROM_SYNTHESIZER,    "Synthesizer",    0x4000, "Konami's Synthesizer"); // officially plain 32K
	init(ROM_PLAYBALL,       "PlayBall",       0x4000, "Sony's PlayBall"); // officially plain 32K
	init(ROM_NETTOU_YAKYUU,  "NettouYakyuu",   0x2000, "Nettou Yakuu");
	init(ROM_HOLY_QURAN,     "AlQuranDecoded", 0x2000, "Holy Qu'ran (pre-decrypted)");
	init(ROM_HOLY_QURAN2,    "AlQuran",        0x2000, "Holy Qu'ran");
	init(ROM_PADIAL8,        "Padial8",        0x2000, "Padial 8kB");
	init(ROM_PADIAL16,       "Padial16",       0x4000, "Padial 16kB");
	init(ROM_SUPERLODERUNNER,"SuperLodeRunner",0x4000, "Super Lode Runner");
	init(ROM_SUPERSWANGI,    "SuperSwangi"    ,0x4000, "Super Swangi");
	init(ROM_MSXDOS2,        "MSXDOS2",        0x4000, "MSX-DOS2");
	init(ROM_MITSUBISHIMLTS2,"MitsubishiMLTS2",0x2000, "Mitsubishi ML-TS2 firmware");
	init(ROM_MANBOW2,        "Manbow2",        0x2000, "Manbow2");
	init(ROM_MANBOW2_2,      "Manbow2_2",      0x2000, "Manbow2 - Second Release");
	init(ROM_HAMARAJANIGHT,  "HamarajaNight",  0x2000, "Best of Hamaraja Night");
	init(ROM_MEGAFLASHROMSCC,"MegaFlashRomScc",0x2000, "Mega Flash ROM SCC");
	init(ROM_MATRAINK,       "MatraInk",       0x0000, "Matra Ink");
	init(ROM_MATRACOMPILATION,"MatraCompilation",0x2000, "Matra Compilation");
	init(ROM_ARC,            "Arc",            0x4000, "Parallax' ARC"); // officially plain 32K
	init(ROM_ROMHUNTERMK2,   "ROMHunterMk2",   0x0000, "ROM Hunter Mk2");
	init(ROM_DOOLY,          "Dooly",          0x4000, "Baby Dinosaur Dooly"); // officially 32K blocksize, but spread over 2 pages
	init(ROM_MSXTRA,         "MSXtra",         0x0000, "PTC MSXtra");
	init(ROM_MSXWRITE,       "MSXWrite",       0x4000, "Japanese MSX Write");
	init(ROM_MULTIROM,       "MultiRom",       0x0000, "MultiRom Collection");
	init(ROM_RAMFILE,        "RAMFILE",        0x0000, "Tecall MSX RAMFILE");
	init(ROM_COLECOMEGACART, "ColecoMegaCart", 0x4000, "ColecoVision MegaCart");
	init(ROM_MEGAFLASHROMSCCPLUS,"MegaFlashRomSccPlus",0x0000, "Mega Flash ROM SCC Plus");
	init(ROM_REPRO_CARTRIDGE1,"ReproCartridgeV1",0x0000, "Repro Cartridge V1");
	init(ROM_REPRO_CARTRIDGE2,"ReproCartridgeV2",0x0000, "Repro Cartridge V2");
	init(ROM_KONAMI_ULTIMATE_COLLECTION,"KonamiUltimateCollection",0x0000, "Konami Ultimate Collection");

	// ROM mapper types used for system ROMs in machines
	init(ROM_PANASONIC, "Panasonic", 0x2000, "Panasonic internal mapper");
	init(ROM_NATIONAL,  "National",  0x4000, "National internal mapper");
	init(ROM_FSA1FM1,   "FSA1FM1",   0x0000, "Panasonic FS-A1FM internal mapper 1"); // TODO: romblocks debuggable?
	init(ROM_FSA1FM2,   "FSA1FM2",   0x2000, "Panasonic FS-A1FM internal mapper 2");
	init(ROM_DRAM,      "DRAM",      0x2000, "MSXturboR DRAM");

	// Non-mapper ROM types
	init(ROM_MIRRORED,    "Mirrored",    0x2000, "Plain rom, mirrored (any size)");
	init(ROM_MIRRORED0000,"Mirrored0000",0x2000, "Plain rom, mirrored start at 0x0000");
	init(ROM_MIRRORED4000,"Mirrored4000",0x2000, "Plain rom, mirrored start at 0x4000");
	init(ROM_MIRRORED8000,"Mirrored8000",0x2000, "Plain rom, mirrored start at 0x8000");
	init(ROM_MIRROREDC000,"MirroredC000",0x2000, "Plain rom, mirrored start at 0xC000");
	init(ROM_NORMAL,      "Normal",      0x2000, "Plain rom (any size)");
	init(ROM_NORMAL0000,  "Normal0000",  0x2000, "Plain rom start at 0x0000");
	init(ROM_NORMAL4000,  "Normal4000",  0x2000, "Plain rom start at 0x4000");
	init(ROM_NORMAL8000,  "Normal8000",  0x2000, "Plain rom start at 0x8000");
	init(ROM_NORMALC000,  "NormalC000",  0x2000, "Plain rom start at 0xC000");
	init(ROM_PAGE0,       "Page0",       0x2000, "Plain 16kB page 0");
	init(ROM_PAGE1,       "Page1",       0x2000, "Plain 16kB page 1");
	init(ROM_PAGE2,       "Page2",       0x2000, "Plain 16kB page 2 (BASIC)");
	init(ROM_PAGE3,       "Page3",       0x2000, "Plain 16kB page 3");
	init(ROM_PAGE01,      "Page01",      0x2000, "Plain 32kB page 0-1");
	init(ROM_PAGE12,      "Page12",      0x2000, "Plain 32kB page 1-2");
	init(ROM_PAGE23,      "Page23",      0x2000, "Plain 32kB page 2-3");
	init(ROM_PAGE012,     "Page012",     0x2000, "Plain 48kB page 0-2");
	init(ROM_PAGE123,     "Page123",     0x2000, "Plain 48kB page 1-3");
	init(ROM_PAGE0123,    "Page0123",    0x2000, "Plain 64kB");

	// Alternative names for rom types, mainly for backwards compatibility
	initAlias(ROM_GENERIC_8KB, "0");
	initAlias(ROM_GENERIC_8KB, "GenericKonami"); // probably actually used in a Zemina Box
	initAlias(ROM_GENERIC_16KB,"1");
	initAlias(ROM_KONAMI_SCC,  "2");
	initAlias(ROM_KONAMI_SCC,  "SCC");
	initAlias(ROM_KONAMI_SCC,  "KONAMI5");
	initAlias(ROM_KONAMI,      "KONAMI4");
	initAlias(ROM_KONAMI,      "3");
	initAlias(ROM_ASCII8,      "4");
	initAlias(ROM_ASCII16,     "5");
	initAlias(ROM_MIRRORED,    "64kB");
	initAlias(ROM_MIRRORED,    "Plain");
	initAlias(ROM_NORMAL0000,  "0x0000");
	initAlias(ROM_NORMAL4000,  "0x4000");
	initAlias(ROM_NORMAL8000,  "0x8000");
	initAlias(ROM_NORMALC000,  "0xC000");
	initAlias(ROM_ASCII16_2,   "HYDLIDE2");
	initAlias(ROM_GAME_MASTER2,"RC755");
	initAlias(ROM_NORMAL8000,  "ROMBAS");
	initAlias(ROM_R_TYPE,      "RTYPE");
	initAlias(ROM_ZEMINA80IN1, "KOREAN80IN1");
	initAlias(ROM_ZEMINA90IN1, "KOREAN90IN1");
	initAlias(ROM_ZEMINA126IN1,"KOREAN126IN1");
	initAlias(ROM_HOLY_QURAN,  "HolyQuran");

	ranges::sort(romTypeInfoMap, RomTypeInfoMapLess());
}
static const RomTypeMap& getRomTypeMap()
{
	init();
	return romTypeMap;
}
static const RomTypeInfoMap& getRomTypeInfoMap()
{
	init();
	return romTypeInfoMap;
}

RomType RomInfo::nameToRomType(string_view name)
{
	auto v = lookup(getRomTypeMap(), name);
	return v ? removeAlias(*v) : ROM_UNKNOWN;
}

string_view RomInfo::romTypeToName(RomType type)
{
	assert(!isAlias(type));
	for (const auto& [name, romType] : getRomTypeMap()) {
		if (romType == type) {
			return name;
		}
	}
	UNREACHABLE; return {};
}

vector<string_view> RomInfo::getAllRomTypes()
{
	vector<string_view> result;
	for (const auto& [name, romType] : getRomTypeMap()) {
		if (!isAlias(romType)) {
			result.push_back(name);
		}
	}
	return result;
}

string_view RomInfo::getDescription(RomType type)
{
	auto& m = getRomTypeInfoMap();
	auto it = ranges::lower_bound(m, type, RomTypeInfoMapLess());
	assert(it != end(m));
	assert(std::get<0>(*it) == type);
	return std::get<1>(*it);
}

unsigned RomInfo::getBlockSize(RomType type)
{
	auto& m = getRomTypeInfoMap();
	auto it = ranges::lower_bound(m, type, RomTypeInfoMapLess());
	assert(it != end(m));
	assert(std::get<0>(*it) == type);
	return std::get<2>(*it);
}

} // namespace openmsx
