// $Id$

#include "RomInfo.hh"
#include "Rom.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "File.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include <map>

using std::auto_ptr;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

typedef map<string, RomType, StringOp::caseless> RomTypeMap;
static RomTypeMap romtype;
static bool init = false;

typedef map<string, RomInfo*, StringOp::caseless> DBMap;

static void initMap()
{
	if (init) {
		return;
	}
	init = true;

	// generic ROM types that don't exist in real ROMs
	// (should not occur in any database!)
	romtype["8kB"]         = ROM_GENERIC_8KB;
	romtype["16kB"]        = ROM_GENERIC_16KB;

	// ROM mapper types for normal software (mainly games)
	romtype["Konami"]      = ROM_KONAMI;
	romtype["KonamiSCC"]   = ROM_KONAMI_SCC;
	romtype["ASCII8"]      = ROM_ASCII8;
	romtype["ASCII16"]     = ROM_ASCII16;
	romtype["R-Type"]      = ROM_R_TYPE;
	romtype["CrossBlaim"]  = ROM_CROSS_BLAIM;
	romtype["HarryFox"]    = ROM_HARRY_FOX;
	romtype["Halnote"]     = ROM_HALNOTE;
	romtype["Zemina80in1"] = ROM_ZEMINA80IN1;
	romtype["Zemina90in1"] = ROM_ZEMINA90IN1;
	romtype["Zemina126in1"]= ROM_ZEMINA126IN1;
	romtype["ASCII16SRAM2"]= ROM_ASCII16_2;
	romtype["ASCII8SRAM8"] = ROM_ASCII8_8;
	romtype["KoeiSRAM8"]   = ROM_KOEI_8;
	romtype["KoeiSRAM32"]  = ROM_KOEI_32;
	romtype["Wizardry"]    = ROM_WIZARDRY;
	romtype["GameMaster2"] = ROM_GAME_MASTER2;
	romtype["Majutsushi"]  = ROM_MAJUTSUSHI;
	romtype["Synthesizer"] = ROM_SYNTHESIZER;
	romtype["HolyQuran"]   = ROM_HOLY_QURAN;

	// ROM mapper types used for system ROMs in machines
	romtype["Panasonic"]   = ROM_PANASONIC;
	romtype["National"]    = ROM_NATIONAL;
	romtype["FSA1FM1"]     = ROM_FSA1FM1;
	romtype["FSA1FM2"]     = ROM_FSA1FM2;
	romtype["DRAM"]        = ROM_DRAM;

	// ROM mapper types used for system ROMs in extensions
	romtype["MSX-AUDIO"]   = ROM_MSX_AUDIO;

	// non-mapper ROM types
	romtype["Mirrored"]    = ROM_MIRRORED;
	romtype["Mirrored0000"]= ROM_MIRRORED0000;
	romtype["Mirrored4000"]= ROM_MIRRORED4000;
	romtype["Mirrored8000"]= ROM_MIRRORED8000;
	romtype["MirroredC000"]= ROM_MIRROREDC000;
	romtype["Normal"]      = ROM_NORMAL;
	romtype["Normal0000"]  = ROM_NORMAL0000;
	romtype["Normal4000"]  = ROM_NORMAL4000;
	romtype["Normal8000"]  = ROM_NORMAL8000;
	romtype["NormalC000"]  = ROM_NORMALC000;
	romtype["Page0"]       = ROM_PAGE0;
	romtype["Page01"]      = ROM_PAGE01;
	romtype["Page012"]     = ROM_PAGE012;
	romtype["Page0123"]    = ROM_PAGE0123;
	romtype["Page1"]       = ROM_PAGE1;
	romtype["Page12"]      = ROM_PAGE12;
	romtype["Page123"]     = ROM_PAGE123;
	romtype["Page2"]       = ROM_PAGE2;
	romtype["Page23"]      = ROM_PAGE23;
	romtype["Page3"]       = ROM_PAGE3;
}

RomInfo::RomInfo(const string& ntitle,   const string& nyear,
                 const string& ncompany, const string& ncountry,
                 const string& nremark,  const RomType& nromType)
{
	title = ntitle;
	year = nyear;
	company = ncompany;
	country = ncountry;
	remark = nremark;
	romType = nromType;
}

RomType RomInfo::nameToRomType(string name)
{
	initMap();

	typedef map<string, string, StringOp::caseless> AliasMap;
	static AliasMap aliasMap;
	static bool aliasMapInit = false;
	if (!aliasMapInit) {
		// alternative names for rom types, mainly for
		// backwards compatibility
		// map from 'alternative' to 'standard' name
		aliasMapInit = true;
		aliasMap["0"]            = "8kB";
		aliasMap["1"]            = "16kB";
		aliasMap["MSXDOS2"]      = "16kB"; // for now
		aliasMap["2"]            = "KonamiSCC";
		aliasMap["SCC"]          = "KonamiSCC";
		aliasMap["KONAMI5"]      = "KonamiSCC";
		aliasMap["KONAMI4"]      = "Konami";
		aliasMap["3"]            = "Konami";
		aliasMap["4"]            = "ASCII8";
		aliasMap["5"]            = "ASCII16";
		aliasMap["64kB"]         = "Mirrored";
		aliasMap["Plain"]        = "Mirrored";
		aliasMap["0x0000"]       = "Normal0000";
		aliasMap["0x4000"]       = "Normal4000";
		aliasMap["0x8000"]       = "Normal8000";
		aliasMap["0xC000"]       = "NormalC000";
		aliasMap["HYDLIDE2"]     = "ASCII16SRAM2";
		aliasMap["RC755"]        = "GameMaster2";
		aliasMap["ROMBAS"]       = "Normal8000";
		aliasMap["RTYPE"]        = "R-Type";
		aliasMap["KOREAN80IN1"]  = "Zemina80in1";
		aliasMap["KOREAN90IN1"]  = "Zemina90in1";
		aliasMap["KOREAN126IN1"] = "Zemina126in1";
	}
	AliasMap::const_iterator alias_it = aliasMap.find(name);
	if (alias_it != aliasMap.end()) {
		name = alias_it->second;
		assert(romtype.find(name) != romtype.end());
	}
	RomTypeMap::const_iterator it = romtype.find(name);
	if (it == romtype.end()) {
		return ROM_UNKNOWN;
	}
	return it->second;
}

void RomInfo::getAllRomTypes(set<string>& result)
{
	initMap();
	for (RomTypeMap::const_iterator it = romtype.begin();
	     it != romtype.end(); ++it) {
		result.insert(it->first);
	}
}

static string parseRemarks(const XMLElement& elem)
{
	string result;
	XMLElement::Children remarks;
	elem.getChildren("remark", remarks);
	for (XMLElement::Children::const_iterator it = remarks.begin();
	     it != remarks.end(); ++it) {
		const XMLElement& remark = **it;
		XMLElement::Children texts;
		remark.getChildren("text", texts);
		for (XMLElement::Children::const_iterator it = texts.begin();
		     it != texts.end(); ++it) {
			// TODO language attribute is ignored
			result += (*it)->getData() + '\n';
		}
	}
	return result;
}

static void addEntry(CliComm& cliComm, RomInfo* romInfo,
                     const string& sha1, DBMap& result)
{
	if (result.find(sha1) == result.end()) {
		result[sha1] = romInfo;
	} else {
		cliComm.printWarning(
			"duplicate softwaredb entry SHA1: " + sha1);
	}
}

static void parseEntry(CliComm& cliComm,
	const XMLElement& rom, DBMap& result,
	const string& title,   const string& year,
	const string& company, const string& country,
	const string& remark,  const string& type)
{
	RomType romType = RomInfo::nameToRomType(type);
	RomInfo* romInfo = new RomInfo(title, year, company, country,
	                               remark, romType);

	XMLElement::Children hashTags;
	rom.getChildren("hash", hashTags);
	for (XMLElement::Children::const_iterator it2 = hashTags.begin();
	     it2 != hashTags.end(); ++it2) {
		if ((*it2)->getAttribute("algo") != "sha1") {
			continue;
		}
		string sha1 = (*it2)->getData();
		addEntry(cliComm, romInfo, sha1, result);
	}
}

static string parseStart(const XMLElement& rom)
{
	string start = rom.getChildData("start", "");
	if      (start == "0x0000") return "0000";
	else if (start == "0x4000") return "4000";
	else if (start == "0x8000") return "8000";
	else if (start == "0xC000") return "C000";
	else return "";
}

static void parseDB(CliComm& cliComm, const XMLElement& doc, DBMap& result)
{
	const XMLElement::Children& children = doc.getChildren();
	for (XMLElement::Children::const_iterator it1 = children.begin();
	     it1 != children.end(); ++it1) {
		// Parse all <software> tags
		const XMLElement& soft = **it1;

		const XMLElement* system = soft.findChild("system");
		if (system && (system->getData() != "MSX")) {
			// skip non-MSX entries
			continue;
		}

		// TODO there can be multiple title tags
		string title   = soft.getChildData("title", "");
		string year    = soft.getChildData("year", "");
		string company = soft.getChildData("company", "");
		string country = soft.getChildData("country", "");
		string remark  = parseRemarks(soft);

		XMLElement::Children dumps;
		soft.getChildren("dump", dumps);
		for (XMLElement::Children::const_iterator it2 = dumps.begin();
		     it2 != dumps.end(); ++it2) {
			const XMLElement& dump = **it2;
			if (const XMLElement* megarom = dump.findChild("megarom")) {
				parseEntry(cliComm, *megarom, result, title, year,
				           company, country, remark,
				           megarom->getChildData("type"));
			} else if (const XMLElement* rom = dump.findChild("rom")) {
				string type = rom->getChildData("type", "Mirrored");
				if (type == "Normal") {
					type += parseStart(*rom);
				} else if (type == "Mirrored") {
					type += parseStart(*rom);
				}
				parseEntry(cliComm, *rom, result, title, year,
				           company, country, remark, type);
			}
		}
	}
}

static auto_ptr<XMLElement> openDB(CliComm& cliComm, const string& filename,
                                   const string& type)
{
	auto_ptr<XMLElement> doc;
	try {
		File file(filename);
		doc = XMLLoader::loadXML(file.getLocalName(), type);
	} catch (FileException& e) {
		// couldn't read file
	} catch (XMLException& e) {
		cliComm.printWarning(
			"Could not parse ROM DB: " + e.getMessage() + "\n"
			"Romtype detection might fail because of this.");
	}
	return doc;
}

auto_ptr<RomInfo> RomInfo::searchRomDB(CliComm& cliComm, const Rom& rom)
{
	// TODO: Turn ROM DB into a separate class.
	// TODO - mem leak on duplicate entries
	//      - mem not freed on exit
	//      - RomInfo is copied only to make ownership managment easier
	//  -->  boost::shared_ptr would solve all these issues
	static DBMap romDBSHA1;
	static bool init = false;

	if (!init) {
		init = true;
		SystemFileContext context;
		const vector<string>& paths = context.getPaths();
		for (vector<string>::const_iterator it = paths.begin();
		     it != paths.end(); ++it) {
			auto_ptr<XMLElement> doc(
				openDB(cliComm, *it + "softwaredb.xml",
				       "softwaredb1.dtd"));
			if (doc.get()) {
				DBMap tmp;
				parseDB(cliComm, *doc, tmp);
				romDBSHA1.insert(tmp.begin(), tmp.end());
			}
		}
		if (romDBSHA1.empty()) {
			cliComm.printWarning(
				"Couldn't load rom database.\n"
				"Romtype detection might fail because of this.");
		}
	}

	int size = rom.getSize();
	if (size == 0) {
		return auto_ptr<RomInfo>(
			new RomInfo("", "", "", "", "Empty ROM", ROM_UNKNOWN));
	}

	const string& sha1sum = rom.getSHA1Sum();
	if (romDBSHA1.find(sha1sum) != romDBSHA1.end()) {
		romDBSHA1[sha1sum]->print(cliComm);
		// Return a copy of the DB entry.
		return auto_ptr<RomInfo>(new RomInfo(*romDBSHA1[sha1sum]));
	}

	// no match found
	return auto_ptr<RomInfo>(NULL);
}

auto_ptr<RomInfo> RomInfo::fetchRomInfo(CliComm& cliComm, const Rom& rom)
{
	// Look for the ROM in the ROM DB.
	auto_ptr<RomInfo> info(searchRomDB(cliComm, rom));
	if (!info.get()) {
		info.reset(new RomInfo("", "", "", "", "", ROM_UNKNOWN));
	}
	return info;
}

void RomInfo::print(CliComm& cliComm)
{
	string year(getYear());
	if (year.empty()) {
		year = "(info not available)";
	}
	string company(getCompany());
	if (company.empty()) {
		company = "(info not available)";
	}
	string info = "Found this ROM in the database:\n"
	              "  Title:    " + getTitle() + "\n"
	              "  Year:     " + year + "\n"
	              "  Company:  " + company + "\n"
	              "  Country:  " + country;
	if (!getRemark().empty()) {
		info += "\n  Remark:   " + getRemark();
	}
	cliComm.printInfo(info);
}

} // namespace openmsx

