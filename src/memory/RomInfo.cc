#include "RomInfo.hh"

#include "MinimalPerfectHash.hh"
#include "StringOp.hh"
#include "stl.hh"

#include <array>
#include <cassert>

namespace openmsx {

using enum RomType;

static constexpr auto romTypeInfoArray = [] {
	array_with_enum_index<RomType, RomInfo::RomTypeInfo> r = {};

	// Generic ROM types that don't exist in real ROMs
	// (should not occur in any database!)
	r[GENERIC_8KB]     = {0x2000, "8kB",             "Generic 8kB"};
	r[GENERIC_16KB]    = {0x4000, "16kB",            "Generic 16kB"};

	// ROM mapper types for normal software (mainly games)
	r[KONAMI]          = {0x2000, "Konami",          "Konami MegaROM"};
	r[KONAMI_SCC]      = {0x2000, "KonamiSCC",       "Konami with SCC"};
	r[KBDMASTER]       = {0x4000, "KeyboardMaster",  "Konami Keyboard Master with VLM5030"}; // officially plain 16K
	r[ASCII8]          = {0x2000, "ASCII8",          "ASCII 8kB"};
	r[ASCII16]         = {0x4000, "ASCII16",         "ASCII 16kB"};
	r[ASCII16X]        = {0x4000, "ASCII16-X",       "ASCII-X 16kB"};
	r[R_TYPE]          = {0x4000, "R-Type",          "R-Type"};
	r[CROSS_BLAIM]     = {0x4000, "CrossBlaim",      "Cross Blaim"};
	r[HARRY_FOX]       = {0x4000, "HarryFox",        "Harry Fox"};
	r[HALNOTE]         = {0x2000, "Halnote",         "Halnote"};
	r[ZEMINA25IN1]     = {0x2000, "Zemina25in1",     "Zemina 25 in 1"};
	r[ZEMINA80IN1]     = {0x2000, "Zemina80in1",     "Zemina 80 in 1"};
	r[ZEMINA90IN1]     = {0x2000, "Zemina90in1",     "Zemina 90 in 1"};
	r[ZEMINA126IN1]    = {0x2000, "Zemina126in1",    "Zemina 126 in 1"};
	r[ASCII16_2]       = {0x4000, "ASCII16SRAM2",    "ASCII 16kB with 2kB SRAM"};
	r[ASCII16_8]       = {0x4000, "ASCII16SRAM8",    "ASCII 16kB with 8kB SRAM"};
	r[ASCII8_8]        = {0x2000, "ASCII8SRAM8",     "ASCII 8kB with 8kB SRAM"};
	r[ASCII8_32]       = {0x2000, "ASCII8SRAM32",    "ASCII 8kB with 32kB SRAM"};
	r[ASCII8_2]        = {0x2000, "ASCII8SRAM2",     "ASCII 8kB with 2kB SRAM"};
	r[KOEI_8]          = {0x2000, "KoeiSRAM8",       "Koei with 8kB SRAM"};
	r[KOEI_32]         = {0x2000, "KoeiSRAM32",      "Koei with 32kB SRAM"};
	r[WIZARDRY]        = {0x2000, "Wizardry",        "Wizardry"};
	r[GAME_MASTER2]    = {0x1000, "GameMaster2",     "Konami's Game Master 2"};
	r[MAJUTSUSHI]      = {0x2000, "Majutsushi",      "Hai no Majutsushi"};
	r[SYNTHESIZER]     = {0     , "Synthesizer",     "Konami's Synthesizer"}; // plain 32K
	r[PLAYBALL]        = {0     , "PlayBall",        "Sony's PlayBall"}; // plain 32K
	r[NETTOU_YAKYUU]   = {0x2000, "NettouYakyuu",    "Nettou Yakuu"};
	r[HOLY_QURAN]      = {0x2000, "AlQuranDecoded",  "Holy Qu'ran (pre-decrypted)"};
	r[HOLY_QURAN2]     = {0x2000, "AlQuran",         "Holy Qu'ran"};
	r[PADIAL8]         = {0x2000, "Padial8",         "Padial 8kB"};
	r[PADIAL16]        = {0x4000, "Padial16",        "Padial 16kB"};
	r[SUPERLODERUNNER] = {0x4000, "SuperLodeRunner", "Super Lode Runner"};
	r[SUPERSWANGI]     = {0x4000, "SuperSwangi",     "Super Swangi"};
	r[MSXDOS2]         = {0x4000, "MSXDOS2",         "MSX-DOS2"};
	r[MITSUBISHIMLTS2] = {0x2000, "MitsubishiMLTS2", "Mitsubishi ML-TS2 firmware"};
	r[MANBOW2]         = {0x2000, "Manbow2",         "Manbow2"};
	r[MANBOW2_2]       = {0x2000, "Manbow2_2",       "Manbow2 - Second Release"};
	r[RBSC_FLASH_KONAMI_SCC]={0x2000,"RBSC_Flash_KonamiSCC","RBSC 2MB flash, Konami SCC mapper"};
	r[HAMARAJANIGHT]   = {0x2000, "HamarajaNight",   "Best of Hamaraja Night"};
	r[MEGAFLASHROMSCC] = {0x2000, "MegaFlashRomScc", "Mega Flash ROM SCC"};
	r[MATRAINK]        = {0     , "MatraInk",        "Matra Ink"};
	r[MATRACOMPILATION]= {0x2000, "MatraCompilation","Matra Compilation"};
	r[ARC]             = {0     , "Arc",             "Parallax' ARC"}; // plain 32K
	r[ROMHUNTERMK2]    = {0x2000, "ROMHunterMk2",    "ROM Hunter Mk2"}; // variable block size, 8kB smallest
	r[DOOLY]           = {0x4000, "Dooly",           "Baby Dinosaur Dooly"}; // officially 32K blocksize, but spread over 2 pages
	r[MSXTRA]          = {0     , "MSXtra",          "PTC MSXtra"};
	r[MSXWRITE]        = {0x4000, "MSXWrite",        "Japanese MSX Write"};
	r[MULTIROM]        = {0x4000, "MultiRom",        "MultiRom Collection"};
	r[RAMFILE]         = {0     , "RAMFILE",         "Tecall MSX RAMFILE"};
	r[RETROHARD31IN1]  = {0x2000, "RetroHardMultiCart31in1", "RetroHard MultiCart 31 in 1"};
	r[ALALAMIAH30IN1]  = {0x8000, "AlAlamiah30-in-1", "Al Alamiah 30-in-1"};
	r[COLECOMEGACART]  = {0x4000, "ColecoMegaCart",  "ColecoVision MegaCart"};
	r[MEGAFLASHROMSCCPLUS]={0     ,"MegaFlashRomSccPlus","Mega Flash ROM SCC Plus"}; // variable block size
	r[REPRO_CARTRIDGE1]= {0x2000, "ReproCartridgeV1","Repro Cartridge V1"};
	r[REPRO_CARTRIDGE2]= {0x2000, "ReproCartridgeV2","Repro Cartridge V2"};
	r[YAMANOOTO       ]= {0x2000, "Yamanooto",       "Yamanooto"};
	r[KONAMI_ULTIMATE_COLLECTION]={0x2000,"KonamiUltimateCollection","Konami Ultimate Collection"};
	r[NEO8]            = {0x2000, "NEO-8",           "NEO-8 mapper"};
	r[NEO16]           = {0x4000, "NEO-16",          "NEO-16 mapper"};
	r[WONDERKID]       = {0x4000, "WonderKid",       "Wonder Kid mapper"};

	// ROM mapper types used for system ROMs in machines
	r[PANASONIC]       = {0x2000, "Panasonic",       "Panasonic internal mapper"};
	r[NATIONAL]        = {0x4000, "National",        "National internal mapper"};
	r[FSA1FM1]         = {0     , "FSA1FM1",         "Panasonic FS-A1FM internal mapper 1"}; // TODO: romblocks debuggable?
	r[FSA1FM2]         = {0x2000, "FSA1FM2",         "Panasonic FS-A1FM internal mapper 2"};
	r[DRAM]            = {0x2000, "DRAM",            "MSXturboR DRAM"};

	// Non-mapper ROM types
	r[MIRRORED]        = {0,      "Mirrored",        "Plain rom, mirrored (any size)"};
	r[MIRRORED0000]    = {0,      "Mirrored0000",    "Plain rom, mirrored start at 0x0000"};
	r[MIRRORED4000]    = {0,      "Mirrored4000",    "Plain rom, mirrored start at 0x4000"};
	r[MIRRORED8000]    = {0,      "Mirrored8000",    "Plain rom, mirrored start at 0x8000"};
	r[MIRROREDC000]    = {0,      "MirroredC000",    "Plain rom, mirrored start at 0xC000"};
	r[NORMAL]          = {0,      "Normal",          "Plain rom (any size)"};
	r[NORMAL0000]      = {0,      "Normal0000",      "Plain rom start at 0x0000"};
	r[NORMAL4000]      = {0,      "Normal4000",      "Plain rom start at 0x4000"};
	r[NORMAL8000]      = {0,      "Normal8000",      "Plain rom start at 0x8000"};
	r[NORMALC000]      = {0,      "NormalC000",      "Plain rom start at 0xC000"};
	r[PAGE0]           = {0,      "Page0",           "Plain 16kB page 0"};
	r[PAGE1]           = {0,      "Page1",           "Plain 16kB page 1"};
	r[PAGE2]           = {0,      "Page2",           "Plain 16kB page 2 (BASIC)"};
	r[PAGE3]           = {0,      "Page3",           "Plain 16kB page 3"};
	r[PAGE01]          = {0,      "Page01",          "Plain 32kB page 0-1"};
	r[PAGE12]          = {0,      "Page12",          "Plain 32kB page 1-2"};
	r[PAGE23]          = {0,      "Page23",          "Plain 32kB page 2-3"};
	r[PAGE012]         = {0,      "Page012",         "Plain 48kB page 0-2"};
	r[PAGE123]         = {0,      "Page123",         "Plain 48kB page 1-3"};
	r[PAGE0123]        = {0,      "Page0123",        "Plain 64kB"};
	return r;
}();
const array_with_enum_index<RomType, RomInfo::RomTypeInfo>& RomInfo::getRomTypeInfo()
{
	return romTypeInfoArray;
}

struct RomTypeAndName {
	RomType romType;
	std::string_view name;
};
static constexpr std::array aliasTable = {
	RomTypeAndName{GENERIC_8KB, "0"},
	RomTypeAndName{GENERIC_8KB, "GenericKonami"}, // probably actually used in a Zemina Box
	RomTypeAndName{GENERIC_16KB,"1"},
	RomTypeAndName{KONAMI_SCC,  "2"},
	RomTypeAndName{KONAMI_SCC,  "SCC"},
	RomTypeAndName{KONAMI_SCC,  "KONAMI5"},
	RomTypeAndName{KONAMI,      "KONAMI4"},
	RomTypeAndName{KONAMI,      "3"},
	RomTypeAndName{ASCII8,      "4"},
	RomTypeAndName{ASCII16,     "5"},
	RomTypeAndName{MIRRORED,    "64kB"},
	RomTypeAndName{MIRRORED,    "Plain"},
	RomTypeAndName{NORMAL0000,  "0x0000"},
	RomTypeAndName{NORMAL4000,  "0x4000"},
	RomTypeAndName{NORMAL8000,  "0x8000"},
	RomTypeAndName{NORMALC000,  "0xC000"},
	RomTypeAndName{ASCII16_2,   "HYDLIDE2"},
	RomTypeAndName{GAME_MASTER2,"RC755"},
	RomTypeAndName{NORMAL8000,  "ROMBAS"},
	RomTypeAndName{R_TYPE,      "RTYPE"},
	RomTypeAndName{ZEMINA80IN1, "KOREAN80IN1"},
	RomTypeAndName{ZEMINA90IN1, "KOREAN90IN1"},
	RomTypeAndName{ZEMINA126IN1,"KOREAN126IN1"},
	RomTypeAndName{HOLY_QURAN,  "HolyQuran"},
};

static constexpr auto combinedRomTable = [] {
	constexpr auto N = std::size(romTypeInfoArray) + std::size(aliasTable);
	std::array<RomTypeAndName, N> result = {};
	size_t i = 0;
	for (const auto& e : romTypeInfoArray) {
		result[i].romType = static_cast<RomType>(i);
		result[i].name = e.name;
		++i;
	}
	for (const auto& e : aliasTable) {
		result[i++] = e;
	}
	return result;
}();

struct RomTypeNameHash {
	[[nodiscard]] static constexpr uint32_t operator()(std::string_view str) {
		constexpr auto MASK = uint8_t(~('a' - 'A')); // case insensitive
		uint32_t d = 0;
		for (char c : str) {
			d = (d ^ (c & MASK)) * 0x01000193;
		}
		return d;
	}
};

// Construct perfect hash function to lookup RomType by name.
static constexpr auto pmh = [] {
	auto getKey = [](size_t i) { return combinedRomTable[i].name; };
	return PerfectMinimalHash::create<std::size(combinedRomTable)>(RomTypeNameHash{}, getKey);
}();

RomType RomInfo::nameToRomType(std::string_view name)
{
	auto idx = pmh.lookupIndex(name);
	assert(idx < std::size(combinedRomTable));
	if (StringOp::casecmp cmp; cmp(combinedRomTable[idx].name, name)) {
		return combinedRomTable[idx].romType;
	}
	return RomType::UNKNOWN;
}

zstring_view RomInfo::romTypeToName(RomType type)
{
	return romTypeInfoArray[type].name;
}

std::string_view RomInfo::getDescription(RomType type)
{
	return romTypeInfoArray[type].description;
}

unsigned RomInfo::getBlockSize(RomType type)
{
	return romTypeInfoArray[type].blockSize;
}

} // namespace openmsx
