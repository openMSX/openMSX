// $Id$

#include <map>
#include <string>
#include "RomInfo.hh"
#include "Rom.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "File.hh"
#include "CliCommOutput.hh"
#include "StringOp.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"

using std::auto_ptr;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

typedef map<string, RomType, StringOp::caseless> RomTypeMap;
static RomTypeMap romtype;
static bool init = false;

static void initMap()
{
	if (init) {
		return;
	}
	init = true;
	
	// generic ROM types that don't exist in real ROMs 
	// (should not occur in any database!)
	romtype["8kB"]         = GENERIC_8KB;
	romtype["16kB"]        = GENERIC_16KB;

	// ROM mapper types for normal software (mainly games)
	romtype["Konami"]      = KONAMI;
	romtype["KonamiSCC"]   = KONAMI_SCC;
	romtype["ASCII8"]      = ASCII8;
	romtype["ASCII16"]     = ASCII16;
	romtype["R-Type"]      = R_TYPE;
	romtype["CrossBlaim"]  = CROSS_BLAIM;
	romtype["HarryFox"]    = HARRY_FOX;
	romtype["Halnote"]     = HALNOTE;
	romtype["Zemina80in1"] = ZEMINA80IN1;
	romtype["Zemina90in1"] = ZEMINA90IN1;
	romtype["Zemina126in1"]= ZEMINA126IN1;
	romtype["ASCII16SRAM2"]= ASCII16_2;
	romtype["ASCII8SRAM8"] = ASCII8_8; 
	romtype["KoeiSRAM8"]   = KOEI_8; 
	romtype["KoeiSRAM32"]  = KOEI_32; 
	romtype["Wizardry"]    = WIZARDRY; 
	romtype["GameMaster2"] = GAME_MASTER2;
	romtype["Majutsushi"]  = MAJUTSUSHI;
	romtype["Synthesizer"] = SYNTHESIZER;
	romtype["HolyQuran"]   = HOLY_QURAN;

	// ROM mapper types used for system ROMs in machines
	romtype["Panasonic"]   = PANASONIC;
	romtype["National"]    = NATIONAL;
	romtype["FSA1FM1"]     = FSA1FM1;
	romtype["FSA1FM2"]     = FSA1FM2;
	romtype["DRAM"]        = DRAM;

	// ROM mapper types used for system ROMs in extensions
	romtype["MSX-AUDIO"]   = MSX_AUDIO;

	// non-mapper ROM types
	romtype["plain"]       = PLAIN;
	romtype["page0"]       = PAGE0;
	romtype["page01"]      = PAGE01;
	romtype["page012"]     = PAGE012;
	romtype["page0123"]    = PAGE0123;
	romtype["page1"]       = PAGE1;
	romtype["page12"]      = PAGE12;
	romtype["page123"]     = PAGE123;
	romtype["page2"]       = PAGE2;
	romtype["page23"]      = PAGE23;
	romtype["page3"]       = PAGE3;
}

RomInfo::RomInfo(const string& ntitle, const string& nyear,
                 const string& ncompany, const string& nremark,
                 const RomType& nromType)
{
	title = ntitle;
	year = nyear;
	company = ncompany;
	remark = nremark;
	romType = nromType;
}

RomInfo::~RomInfo()
{
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
		aliasMap["64kB"]         = "plain";
		aliasMap["HYDLIDE2"]     = "ASCII16SRAM2";
		aliasMap["RC755"]        = "GameMaster2";
		aliasMap["ROMBAS"]       = "page2";
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
		return UNKNOWN;
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

static void parseDB(const XMLElement& doc, map<string, RomInfo*>& result)
{
	const XMLElement::Children& children = doc.getChildren();
	for (XMLElement::Children::const_iterator it1 = children.begin();
	     it1 != children.end(); ++it1) {
		// TODO there can be multiple title tags
		string title   = (*it1)->getChildData("title", "");
		string year    = (*it1)->getChildData("year", "");
		string company = (*it1)->getChildData("company", "");
		string remark  = (*it1)->getChildData("remark", "");
		string romtype = (*it1)->getChildData("romtype", "");
		
		RomInfo* romInfo = new RomInfo(title, year,
		   company, remark, RomInfo::nameToRomType(romtype));
		const XMLElement::Children& sub_children = (*it1)->getChildren();
		for (XMLElement::Children::const_iterator it2 = sub_children.begin();
		     it2 != sub_children.end(); ++it2) {
			if ((*it2)->getName() == "sha1") {
				string sha1 = (*it2)->getData();
				if (result.find(sha1) == result.end()) {
					result[sha1] = romInfo;
				} else {
					CliCommOutput::instance().printWarning(
						"duplicate romdb entry SHA1: " + sha1);
				}
			}
		}
	}
}

auto_ptr<RomInfo> RomInfo::searchRomDB(const Rom& rom)
{
	// TODO: Turn ROM DB into a separate class.
	// TODO - mem leak on duplicate entries
	//      - mem not freed on exit
	//      - RomInfo is copied only to make ownership managment easier
	//  -->  boost::shared_ptr would solve all these issues
	static map<string, RomInfo*> romDBSHA1;
	static bool init = false;

	if (!init) {
		init = true;
		SystemFileContext context;
		const vector<string>& paths = context.getPaths();
		for (vector<string>::const_iterator it = paths.begin();
		     it != paths.end(); ++it) {
			try {
				File file(*it + "romdb.xml");
				auto_ptr<XMLElement> doc(XMLLoader::loadXML(
					file.getLocalName(), "romdb.dtd"));
				map<string, RomInfo*> tmp;
				parseDB(*doc, tmp);
				romDBSHA1.insert(tmp.begin(), tmp.end());
			} catch (FileException& e) {
				// couldn't read file
			} catch (XMLException& e) {
				CliCommOutput::instance().printWarning(
					"Could not parse ROM DB: " + e.getMessage() + "\n"
					"Romtype detection might fail because of this.");
			}
		}
		if (romDBSHA1.empty()) {
			CliCommOutput::instance().printWarning(
				"Couldn't load rom database.\n"
				"Romtype detection might fail because of this.");
		}
	}
	
	int size = rom.getSize();
	if (size == 0) {
		return auto_ptr<RomInfo>(new RomInfo("", "", "", "Empty ROM", UNKNOWN));
	}

	const string& sha1sum = rom.getSHA1Sum();
	if (romDBSHA1.find(sha1sum) != romDBSHA1.end()) {
		romDBSHA1[sha1sum]->print();
		// Return a copy of the DB entry.
		return auto_ptr<RomInfo>(new RomInfo(*romDBSHA1[sha1sum]));
	}

	// no match found
	return auto_ptr<RomInfo>(NULL);
}

auto_ptr<RomInfo> RomInfo::fetchRomInfo(const Rom& rom)
{
	// Look for the ROM in the ROM DB.
	auto_ptr<RomInfo> info(searchRomDB(rom));
	if (!info.get()) {
		info.reset(new RomInfo("", "", "", "", UNKNOWN));
	}
	return info;
}

void RomInfo::print()
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
	              "  Company:  " + company;
	if (!getRemark().empty()) {
		info += "\n  Remark:   " + getRemark();
	}
	CliCommOutput::instance().printInfo(info);
}

} // namespace openmsx

