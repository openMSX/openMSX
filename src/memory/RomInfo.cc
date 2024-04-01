#include "MinimalPerfectHash.hh"
#include "RomInfo.hh"
#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "view.hh"
#include <array>
#include <cassert>

namespace openmsx {

static constexpr auto romTypeInfoArray = [] {
	std::array<RomInfo::RomTypeInfo, RomType::ROM_LAST> r = {};

	// Generic ROM types that don't exist in real ROMs
	// (should not occur in any database!)
	r[ROM_GENERIC_8KB]     = {0x2000, "8kB",             "Generic 8kB"};
	r[ROM_GENERIC_16KB]    = {0x4000, "16kB",            "Generic 16kB"};

	// ROM mapper types for normal software (mainly games)
	r[ROM_KONAMI]          = {0x2000, "Konami",          "Konami MegaROM"};
	r[ROM_KONAMI_SCC]      = {0x2000, "KonamiSCC",       "Konami with SCC"};
	r[ROM_KBDMASTER]       = {0x4000, "KeyboardMaster",  "Konami Keyboard Master with VLM5030"}; // officially plain 16K
	r[ROM_ASCII8]          = {0x2000, "ASCII8",          "ASCII 8kB"};
	r[ROM_ASCII16]         = {0x4000, "ASCII16",         "ASCII 16kB"};
	r[ROM_R_TYPE]          = {0x4000, "R-Type",          "R-Type"};
	r[ROM_CROSS_BLAIM]     = {0x4000, "CrossBlaim",      "Cross Blaim"};
	r[ROM_HARRY_FOX]       = {0x4000, "HarryFox",        "Harry Fox"};
	r[ROM_HALNOTE]         = {0x2000, "Halnote",         "Halnote"};
	r[ROM_ZEMINA25IN1]     = {0x2000, "Zemina25in1",     "Zemina 25 in 1"};
	r[ROM_ZEMINA80IN1]     = {0x2000, "Zemina80in1",     "Zemina 80 in 1"};
	r[ROM_ZEMINA90IN1]     = {0x2000, "Zemina90in1",     "Zemina 90 in 1"};
	r[ROM_ZEMINA126IN1]    = {0x2000, "Zemina126in1",    "Zemina 126 in 1"};
	r[ROM_ASCII16_2]       = {0x4000, "ASCII16SRAM2",    "ASCII 16kB with 2kB SRAM"};
	r[ROM_ASCII16_8]       = {0x4000, "ASCII16SRAM8",    "ASCII 16kB with 8kB SRAM"};
	r[ROM_ASCII8_8]        = {0x2000, "ASCII8SRAM8",     "ASCII 8kB with 8kB SRAM"};
	r[ROM_ASCII8_32]       = {0x2000, "ASCII8SRAM32",    "ASCII 8kB with 32kB SRAM"};
	r[ROM_ASCII8_2]        = {0x2000, "ASCII8SRAM2",     "ASCII 8kB with 2kB SRAM"};
	r[ROM_KOEI_8]          = {0x2000, "KoeiSRAM8",       "Koei with 8kB SRAM"};
	r[ROM_KOEI_32]         = {0x2000, "KoeiSRAM32",      "Koei with 32kB SRAM"};
	r[ROM_WIZARDRY]        = {0x2000, "Wizardry",        "Wizardry"};
	r[ROM_GAME_MASTER2]    = {0x1000, "GameMaster2",     "Konami's Game Master 2"};
	r[ROM_MAJUTSUSHI]      = {0x2000, "Majutsushi",      "Hai no Majutsushi"};
	r[ROM_SYNTHESIZER]     = {0     , "Synthesizer",     "Konami's Synthesizer"}; // plain 32K
	r[ROM_PLAYBALL]        = {0     , "PlayBall",        "Sony's PlayBall"}; // plain 32K
	r[ROM_NETTOU_YAKYUU]   = {0x2000, "NettouYakyuu",    "Nettou Yakuu"};
	r[ROM_HOLY_QURAN]      = {0x2000, "AlQuranDecoded",  "Holy Qu'ran (pre-decrypted)"};
	r[ROM_HOLY_QURAN2]     = {0x2000, "AlQuran",         "Holy Qu'ran"};
	r[ROM_PADIAL8]         = {0x2000, "Padial8",         "Padial 8kB"};
	r[ROM_PADIAL16]        = {0x4000, "Padial16",        "Padial 16kB"};
	r[ROM_SUPERLODERUNNER] = {0x4000, "SuperLodeRunner", "Super Lode Runner"};
	r[ROM_SUPERSWANGI]     = {0x4000, "SuperSwangi",     "Super Swangi"};
	r[ROM_MSXDOS2]         = {0x4000, "MSXDOS2",         "MSX-DOS2"};
	r[ROM_MITSUBISHIMLTS2] = {0x2000, "MitsubishiMLTS2", "Mitsubishi ML-TS2 firmware"};
	r[ROM_MANBOW2]         = {0x2000, "Manbow2",         "Manbow2"};
	r[ROM_MANBOW2_2]       = {0x2000, "Manbow2_2",       "Manbow2 - Second Release"};
	r[ROM_RBSC_FLASH_KONAMI_SCC]={0x2000,"RBSC_Flash_KonamiSCC","RBSC 2MB flash, Konami SCC mapper"};
	r[ROM_HAMARAJANIGHT]   = {0x2000, "HamarajaNight",   "Best of Hamaraja Night"};
	r[ROM_MEGAFLASHROMSCC] = {0x2000, "MegaFlashRomScc", "Mega Flash ROM SCC"};
	r[ROM_MATRAINK]        = {0     , "MatraInk",        "Matra Ink"};
	r[ROM_MATRACOMPILATION]= {0x2000, "MatraCompilation","Matra Compilation"};
	r[ROM_ARC]             = {0     , "Arc",             "Parallax' ARC"}; // plain 32K
	r[ROM_ROMHUNTERMK2]    = {0x2000, "ROMHunterMk2",    "ROM Hunter Mk2"}; // variable block size, 8kB smallest
	r[ROM_DOOLY]           = {0x4000, "Dooly",           "Baby Dinosaur Dooly"}; // officially 32K blocksize, but spread over 2 pages
	r[ROM_MSXTRA]          = {0     , "MSXtra",          "PTC MSXtra"};
	r[ROM_MSXWRITE]        = {0x4000, "MSXWrite",        "Japanese MSX Write"};
	r[ROM_MULTIROM]        = {0x4000, "MultiRom",        "MultiRom Collection"};
	r[ROM_RAMFILE]         = {0     , "RAMFILE",         "Tecall MSX RAMFILE"};
	r[ROM_RETROHARD31IN1]  = {0x2000, "RetroHardMultiCart31in1", "RetroHard MultiCart 31 in 1"};
	r[ROM_ALALAMIAH30IN1]  = {0x8000, "AlAlamiah30-in-1", "Al Alamiah 30-in-1"};
	r[ROM_COLECOMEGACART]  = {0x4000, "ColecoMegaCart",  "ColecoVision MegaCart"};
	r[ROM_MEGAFLASHROMSCCPLUS]={0     ,"MegaFlashRomSccPlus","Mega Flash ROM SCC Plus"}; // variable block size
	r[ROM_REPRO_CARTRIDGE1]= {0x2000, "ReproCartridgeV1","Repro Cartridge V1"};
	r[ROM_REPRO_CARTRIDGE2]= {0x2000, "ReproCartridgeV2","Repro Cartridge V2"};
	r[ROM_KONAMI_ULTIMATE_COLLECTION]={0x2000,"KonamiUltimateCollection","Konami Ultimate Collection"};
	r[ROM_NEO8]            = {0x2000, "NEO-8",           "NEO-8 mapper"};
	r[ROM_NEO16]           = {0x4000, "NEO-16",          "NEO-16 mapper"};

	// ROM mapper types used for system ROMs in machines
	r[ROM_PANASONIC]       = {0x2000, "Panasonic",       "Panasonic internal mapper"};
	r[ROM_NATIONAL]        = {0x4000, "National",        "National internal mapper"};
	r[ROM_FSA1FM1]         = {0     , "FSA1FM1",         "Panasonic FS-A1FM internal mapper 1"}; // TODO: romblocks debuggable?
	r[ROM_FSA1FM2]         = {0x2000, "FSA1FM2",         "Panasonic FS-A1FM internal mapper 2"};
	r[ROM_DRAM]            = {0x2000, "DRAM",            "MSXturboR DRAM"};

	// Non-mapper ROM types
	r[ROM_MIRRORED]        = {0,      "Mirrored",        "Plain rom, mirrored (any size)"};
	r[ROM_MIRRORED0000]    = {0,      "Mirrored0000",    "Plain rom, mirrored start at 0x0000"};
	r[ROM_MIRRORED4000]    = {0,      "Mirrored4000",    "Plain rom, mirrored start at 0x4000"};
	r[ROM_MIRRORED8000]    = {0,      "Mirrored8000",    "Plain rom, mirrored start at 0x8000"};
	r[ROM_MIRROREDC000]    = {0,      "MirroredC000",    "Plain rom, mirrored start at 0xC000"};
	r[ROM_NORMAL]          = {0,      "Normal",          "Plain rom (any size)"};
	r[ROM_NORMAL0000]      = {0,      "Normal0000",      "Plain rom start at 0x0000"};
	r[ROM_NORMAL4000]      = {0,      "Normal4000",      "Plain rom start at 0x4000"};
	r[ROM_NORMAL8000]      = {0,      "Normal8000",      "Plain rom start at 0x8000"};
	r[ROM_NORMALC000]      = {0,      "NormalC000",      "Plain rom start at 0xC000"};
	r[ROM_PAGE0]           = {0,      "Page0",           "Plain 16kB page 0"};
	r[ROM_PAGE1]           = {0,      "Page1",           "Plain 16kB page 1"};
	r[ROM_PAGE2]           = {0,      "Page2",           "Plain 16kB page 2 (BASIC)"};
	r[ROM_PAGE3]           = {0,      "Page3",           "Plain 16kB page 3"};
	r[ROM_PAGE01]          = {0,      "Page01",          "Plain 32kB page 0-1"};
	r[ROM_PAGE12]          = {0,      "Page12",          "Plain 32kB page 1-2"};
	r[ROM_PAGE23]          = {0,      "Page23",          "Plain 32kB page 2-3"};
	r[ROM_PAGE012]         = {0,      "Page012",         "Plain 48kB page 0-2"};
	r[ROM_PAGE123]         = {0,      "Page123",         "Plain 48kB page 1-3"};
	r[ROM_PAGE0123]        = {0,      "Page0123",        "Plain 64kB"};
	return r;
}();
const std::array<RomInfo::RomTypeInfo, RomType::ROM_LAST>& RomInfo::getRomTypeInfo()
{
	return romTypeInfoArray;
}

struct RomTypeAndName {
	RomType romType;
	std::string_view name;
};
static constexpr std::array aliasTable = {
	RomTypeAndName{ROM_GENERIC_8KB, "0"},
	RomTypeAndName{ROM_GENERIC_8KB, "GenericKonami"}, // probably actually used in a Zemina Box
	RomTypeAndName{ROM_GENERIC_16KB,"1"},
	RomTypeAndName{ROM_KONAMI_SCC,  "2"},
	RomTypeAndName{ROM_KONAMI_SCC,  "SCC"},
	RomTypeAndName{ROM_KONAMI_SCC,  "KONAMI5"},
	RomTypeAndName{ROM_KONAMI,      "KONAMI4"},
	RomTypeAndName{ROM_KONAMI,      "3"},
	RomTypeAndName{ROM_ASCII8,      "4"},
	RomTypeAndName{ROM_ASCII16,     "5"},
	RomTypeAndName{ROM_MIRRORED,    "64kB"},
	RomTypeAndName{ROM_MIRRORED,    "Plain"},
	RomTypeAndName{ROM_NORMAL0000,  "0x0000"},
	RomTypeAndName{ROM_NORMAL4000,  "0x4000"},
	RomTypeAndName{ROM_NORMAL8000,  "0x8000"},
	RomTypeAndName{ROM_NORMALC000,  "0xC000"},
	RomTypeAndName{ROM_ASCII16_2,   "HYDLIDE2"},
	RomTypeAndName{ROM_GAME_MASTER2,"RC755"},
	RomTypeAndName{ROM_NORMAL8000,  "ROMBAS"},
	RomTypeAndName{ROM_R_TYPE,      "RTYPE"},
	RomTypeAndName{ROM_ZEMINA80IN1, "KOREAN80IN1"},
	RomTypeAndName{ROM_ZEMINA90IN1, "KOREAN90IN1"},
	RomTypeAndName{ROM_ZEMINA126IN1,"KOREAN126IN1"},
	RomTypeAndName{ROM_HOLY_QURAN,  "HolyQuran"},
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
	[[nodiscard]] constexpr uint32_t operator()(std::string_view str) const {
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
	StringOp::casecmp cmp;
	if (cmp(combinedRomTable[idx].name, name)) {
		return combinedRomTable[idx].romType;
	}
	return ROM_UNKNOWN;
}

std::string_view RomInfo::romTypeToName(RomType type)
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
