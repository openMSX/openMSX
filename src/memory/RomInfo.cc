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

typedef map<string, RomType, StringOp::caseless> RomTypeMap;

static const RomTypeMap& getRomTypeMap()
{
	static bool init = false;
	static RomTypeMap romTypeMap;
	if (!init) {
		init = true;

		// generic ROM types that don't exist in real ROMs
		// (should not occur in any database!)
		romTypeMap["8kB"]             = ROM_GENERIC_8KB;
		romTypeMap["16kB"]            = ROM_GENERIC_16KB;

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
		romTypeMap["MegaFlashRomScc"]= ROM_MEGAFLASHROMSCC;
		romTypeMap["MatraInk"]       = ROM_MATRAINK;
		romTypeMap["Arc"]            = ROM_ARC;
		romTypeMap["MegaFlashRomSccPlus"]= ROM_MEGAFLASHROMSCCPLUS;

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
	}
	return romTypeMap;
}

RomInfo::RomInfo(const string& ntitle,   const string& nyear,
                 const string& ncompany, const string& ncountry,
                 bool noriginal,         const string& norigType,
                 const string& nremark,  const RomType& nromType,
                 int ngenMSXid)
	: title(ntitle)
	, year(nyear)
	, company(ncompany)
	, country(ncountry)
	, origType(norigType)
	, remark(nremark)
	, romType(nromType)
	, genMSXid(ngenMSXid)
	, original(noriginal)
{
}

RomType RomInfo::nameToRomType(string name)
{
	typedef map<string, string, StringOp::caseless> AliasMap;
	static AliasMap aliasMap;
	static bool aliasMapInit = false;
	if (!aliasMapInit) {
		// alternative names for rom types, mainly for
		// backwards compatibility
		// map from 'alternative' to 'standard' name
		aliasMapInit = true;
		aliasMap["0"]             = "8kB";
		aliasMap["GenericKonami"] = "8kB"; // probably actually used in a Zemina Box
		aliasMap["1"]             = "16kB";
		aliasMap["2"]             = "KonamiSCC";
		aliasMap["SCC"]           = "KonamiSCC";
		aliasMap["KONAMI5"]       = "KonamiSCC";
		aliasMap["KONAMI4"]       = "Konami";
		aliasMap["3"]             = "Konami";
		aliasMap["4"]             = "ASCII8";
		aliasMap["5"]             = "ASCII16";
		aliasMap["64kB"]          = "Mirrored";
		aliasMap["Plain"]         = "Mirrored";
		aliasMap["0x0000"]        = "Normal0000";
		aliasMap["0x4000"]        = "Normal4000";
		aliasMap["0x8000"]        = "Normal8000";
		aliasMap["0xC000"]        = "NormalC000";
		aliasMap["HYDLIDE2"]      = "ASCII16SRAM2";
		aliasMap["RC755"]         = "GameMaster2";
		aliasMap["ROMBAS"]        = "Normal8000";
		aliasMap["RTYPE"]         = "R-Type";
		aliasMap["KOREAN80IN1"]   = "Zemina80in1";
		aliasMap["KOREAN90IN1"]   = "Zemina90in1";
		aliasMap["KOREAN126IN1"]  = "Zemina126in1";
		aliasMap["HolyQuran"]     = "AlQuranDecoded";
	}
	const RomTypeMap& romTypeMap = getRomTypeMap();
	AliasMap::const_iterator alias_it = aliasMap.find(name);
	if (alias_it != aliasMap.end()) {
		name = alias_it->second;
		assert(romTypeMap.find(name) != romTypeMap.end());
	}
	RomTypeMap::const_iterator it = romTypeMap.find(name);
	if (it == romTypeMap.end()) {
		return ROM_UNKNOWN;
	}
	return it->second;
}

string RomInfo::romTypeToName(RomType type)
{
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
		result.insert(it->first);
	}
}

} // namespace openmsx

