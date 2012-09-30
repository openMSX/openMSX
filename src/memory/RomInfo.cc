// $Id$

#include "RomInfo.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include <cassert>
#include <map>

using std::map;
using std::set;
using std::string;

namespace openmsx {

typedef map<string_ref, RomType, StringOp::caseless> RomTypeMap;

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

static const RomTypeMap& getRomTypeMap()
{
	static bool init = false;
	static RomTypeMap romTypeMap;
	if (!init) {
		init = true;

		// generic ROM types that don't exist in real ROMs
		// (should not occur in any database!)
		romTypeMap["8kB"]            = ROM_GENERIC_8KB;
		romTypeMap["16kB"]           = ROM_GENERIC_16KB;

		// ROM mapper types for normal software (mainly games)
		romTypeMap["Konami"]         = ROM_KONAMI;
		romTypeMap["KonamiSCC"]      = ROM_KONAMI_SCC;
		romTypeMap["KeyboardMaster"] = ROM_KBDMASTER;
		romTypeMap["ASCII8"]         = ROM_ASCII8;
		romTypeMap["ASCII16"]        = ROM_ASCII16;
		romTypeMap["R-Type"]         = ROM_R_TYPE;
		romTypeMap["CrossBlaim"]     = ROM_CROSS_BLAIM;
		romTypeMap["HarryFox"]       = ROM_HARRY_FOX;
		romTypeMap["Halnote"]        = ROM_HALNOTE;
		romTypeMap["Zemina80in1"]    = ROM_ZEMINA80IN1;
		romTypeMap["Zemina90in1"]    = ROM_ZEMINA90IN1;
		romTypeMap["Zemina126in1"]   = ROM_ZEMINA126IN1;
		romTypeMap["ASCII16SRAM2"]   = ROM_ASCII16_2;
		romTypeMap["ASCII8SRAM8"]    = ROM_ASCII8_8;
		romTypeMap["KoeiSRAM8"]      = ROM_KOEI_8;
		romTypeMap["KoeiSRAM32"]     = ROM_KOEI_32;
		romTypeMap["Wizardry"]       = ROM_WIZARDRY;
		romTypeMap["GameMaster2"]    = ROM_GAME_MASTER2;
		romTypeMap["Majutsushi"]     = ROM_MAJUTSUSHI;
		romTypeMap["Synthesizer"]    = ROM_SYNTHESIZER;
		romTypeMap["PlayBall"]       = ROM_PLAYBALL;
		romTypeMap["NettouYakyuu"]   = ROM_NETTOU_YAKYUU;
		romTypeMap["AlQuranDecoded"] = ROM_HOLY_QURAN;
		romTypeMap["AlQuran"]        = ROM_HOLY_QURAN2;
		romTypeMap["Padial8"]        = ROM_PADIAL8;
		romTypeMap["Padial16"]       = ROM_PADIAL16;
		romTypeMap["SuperLodeRunner"]= ROM_SUPERLODERUNNER;
		romTypeMap["MSXDOS2"]        = ROM_MSXDOS2;
		romTypeMap["Manbow2"]        = ROM_MANBOW2;
		romTypeMap["Manbow2_2"]      = ROM_MANBOW2_2;
		romTypeMap["HamarajaNight"]  = ROM_HAMARAJANIGHT;
		romTypeMap["MegaFlashRomScc"]= ROM_MEGAFLASHROMSCC;
		romTypeMap["MatraInk"]       = ROM_MATRAINK;
		romTypeMap["Arc"]            = ROM_ARC;
		romTypeMap["Dooly"]          = ROM_DOOLY;
		romTypeMap["MegaFlashRomSccPlus"]= ROM_MEGAFLASHROMSCCPLUS;
		romTypeMap["MSXtra"]         = ROM_MSXTRA;

		// ROM mapper types used for system ROMs in machines
		romTypeMap["Panasonic"]      = ROM_PANASONIC;
		romTypeMap["National"]       = ROM_NATIONAL;
		romTypeMap["FSA1FM1"]        = ROM_FSA1FM1;
		romTypeMap["FSA1FM2"]        = ROM_FSA1FM2;
		romTypeMap["DRAM"]           = ROM_DRAM;

		// non-mapper ROM types
		romTypeMap["Mirrored"]       = ROM_MIRRORED;
		romTypeMap["Mirrored0000"]   = ROM_MIRRORED0000;
		romTypeMap["Mirrored4000"]   = ROM_MIRRORED4000;
		romTypeMap["Mirrored8000"]   = ROM_MIRRORED8000;
		romTypeMap["MirroredC000"]   = ROM_MIRROREDC000;
		romTypeMap["Normal"]         = ROM_NORMAL;
		romTypeMap["Normal0000"]     = ROM_NORMAL0000;
		romTypeMap["Normal4000"]     = ROM_NORMAL4000;
		romTypeMap["Normal8000"]     = ROM_NORMAL8000;
		romTypeMap["NormalC000"]     = ROM_NORMALC000;
		romTypeMap["Page0"]          = ROM_PAGE0;
		romTypeMap["Page01"]         = ROM_PAGE01;
		romTypeMap["Page012"]        = ROM_PAGE012;
		romTypeMap["Page0123"]       = ROM_PAGE0123;
		romTypeMap["Page1"]          = ROM_PAGE1;
		romTypeMap["Page12"]         = ROM_PAGE12;
		romTypeMap["Page123"]        = ROM_PAGE123;
		romTypeMap["Page2"]          = ROM_PAGE2;
		romTypeMap["Page23"]         = ROM_PAGE23;
		romTypeMap["Page3"]          = ROM_PAGE3;

		// alternative names for rom types, mainly for
		// backwards compatibility
		romTypeMap["0"]             = makeAlias(ROM_GENERIC_8KB);
		romTypeMap["GenericKonami"] = makeAlias(ROM_GENERIC_8KB); // probably actually used in a Zemina Box
		romTypeMap["1"]             = makeAlias(ROM_GENERIC_16KB);
		romTypeMap["2"]             = makeAlias(ROM_KONAMI_SCC);
		romTypeMap["SCC"]           = makeAlias(ROM_KONAMI_SCC);
		romTypeMap["KONAMI5"]       = makeAlias(ROM_KONAMI_SCC);
		romTypeMap["KONAMI4"]       = makeAlias(ROM_KONAMI);
		romTypeMap["3"]             = makeAlias(ROM_KONAMI);
		romTypeMap["4"]             = makeAlias(ROM_ASCII8);
		romTypeMap["5"]             = makeAlias(ROM_ASCII16);
		romTypeMap["64kB"]          = makeAlias(ROM_MIRRORED);
		romTypeMap["Plain"]         = makeAlias(ROM_MIRRORED);
		romTypeMap["0x0000"]        = makeAlias(ROM_NORMAL0000);
		romTypeMap["0x4000"]        = makeAlias(ROM_NORMAL4000);
		romTypeMap["0x8000"]        = makeAlias(ROM_NORMAL8000);
		romTypeMap["0xC000"]        = makeAlias(ROM_NORMALC000);
		romTypeMap["HYDLIDE2"]      = makeAlias(ROM_ASCII16_2);
		romTypeMap["RC755"]         = makeAlias(ROM_GAME_MASTER2);
		romTypeMap["ROMBAS"]        = makeAlias(ROM_NORMAL8000);
		romTypeMap["RTYPE"]         = makeAlias(ROM_R_TYPE);
		romTypeMap["KOREAN80IN1"]   = makeAlias(ROM_ZEMINA80IN1);
		romTypeMap["KOREAN90IN1"]   = makeAlias(ROM_ZEMINA90IN1);
		romTypeMap["KOREAN126IN1"]  = makeAlias(ROM_ZEMINA126IN1);
		romTypeMap["HolyQuran"]     = makeAlias(ROM_HOLY_QURAN);
	}
	return romTypeMap;
}

RomInfo::RomInfo(string_ref ntitle,   string_ref nyear,
                 string_ref ncompany, string_ref ncountry,
                 bool noriginal,      string_ref norigType,
                 string_ref nremark,  const RomType& nromType,
                 int ngenMSXid)
	: title   (ntitle   .data(), ntitle   .size())
	, year    (nyear    .data(), nyear    .size())
	, company (ncompany .data(), ncompany .size())
	, country (ncountry .data(), ncountry .size())
	, origType(norigType.data(), norigType.size())
	, remark  (nremark  .data(), nremark  .size())
	, romType(nromType)
	, genMSXid(ngenMSXid)
	, original(noriginal)
{
}

RomType RomInfo::nameToRomType(string_ref name)
{
	const RomTypeMap& romTypeMap = getRomTypeMap();
	RomTypeMap::const_iterator it = romTypeMap.find(name);
	if (it == romTypeMap.end()) {
		return ROM_UNKNOWN;
	}
	return removeAlias(it->second);
}

string_ref RomInfo::romTypeToName(RomType type)
{
	assert(!isAlias(type));
	const RomTypeMap& romTypeMap = getRomTypeMap();
	for (RomTypeMap::const_iterator it = romTypeMap.begin();
	     it != romTypeMap.end(); ++it) {
		if (it->second == type) {
			return it->first;
		}
	}
	UNREACHABLE; return "";
}

void RomInfo::getAllRomTypes(set<string>& result)
{
	const RomTypeMap& romTypeMap = getRomTypeMap();
	for (RomTypeMap::const_iterator it = romTypeMap.begin();
	     it != romTypeMap.end(); ++it) {
		if (!isAlias(it->second)) {
			result.insert(it->first.str());
		}
	}
}

} // namespace openmsx

