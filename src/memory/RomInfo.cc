// $Id$

#include <map>
#include <string>
#include "RomInfo.hh"
#include "Rom.hh"
#include "xmlx.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "File.hh"
#include "CliCommOutput.hh"
#include "StringOp.hh"

using std::auto_ptr;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

typedef map<string, MapperType, StringOp::caseless> MapperTypeMap;
static MapperTypeMap mappertype;
static bool init = false;

static void initMap()
{
	if (init) {
		return;
	}
	init = true;
	
	mappertype["8kB"]         = GENERIC_8KB;
	mappertype["16kB"]        = GENERIC_16KB;
	mappertype["konami5"]     = KONAMI5;
	mappertype["konami4"]     = KONAMI4;
	mappertype["ascii8"]      = ASCII_8KB;
	mappertype["ascii16"]     = ASCII_16KB;
	mappertype["rtype"]       = R_TYPE;
	mappertype["crossblaim"]  = CROSS_BLAIM;
	mappertype["panasonic"]   = PANASONIC;
	mappertype["national"]    = NATIONAL;
	mappertype["msx-audio"]   = MSX_AUDIO;
	mappertype["harryfox"]    = HARRY_FOX;
	mappertype["halnote"]     = HALNOTE;
	mappertype["korean80in1"] = KOREAN80IN1;
	mappertype["korean90in1"] = KOREAN90IN1;
	mappertype["korean126in1"]= KOREAN126IN1;
	mappertype["holyquran"]   = HOLY_QURAN;
	mappertype["fsa1fm1"]     = FSA1FM1;
	mappertype["fsa1fm2"]     = FSA1FM2;
	mappertype["plain"]       = PLAIN;
	mappertype["dram"]        = DRAM;
	mappertype["hydlide2"]    = HYDLIDE2;
	mappertype["ascii8-8"]    = ASCII8_8; 
	mappertype["koei-8"]      = KOEI_8; 
	mappertype["koei-32"]     = KOEI_32; 
	mappertype["wizardry"]    = WIZARDRY; 
	mappertype["gamemaster2"] = GAME_MASTER2;
	mappertype["majutsushi"]  = MAJUTSUSHI;
	mappertype["synthesizer"] = SYNTHESIZER;
	mappertype["page0"]       = PAGE0;
	mappertype["page01"]      = PAGE01;
	mappertype["page012"]     = PAGE012;
	mappertype["page0123"]    = PAGE0123;
	mappertype["page1"]       = PAGE1;
	mappertype["page12"]      = PAGE12;
	mappertype["page123"]     = PAGE123;
	mappertype["page2"]       = PAGE2;
	mappertype["page23"]      = PAGE23;
	mappertype["page3"]       = PAGE3;
}

RomInfo::RomInfo(const string& ntitle, const string& nyear,
                 const string& ncompany, const string& nremark,
                 const MapperType& nmapperType)
{
	title = ntitle;
	year = nyear;
	company = ncompany;
	remark = nremark;
	mapperType = nmapperType;
}

RomInfo::~RomInfo()
{
}

MapperType RomInfo::nameToMapperType(string name)
{
	initMap();
	
	static map<string, string, StringOp::caseless> aliasMap;
	static bool aliasMapInit = false;
	if (!aliasMapInit) {
		// alternative names for mapper types, mainly for
		// backwards compatibility
		// map from 'alternative' to 'standard' name
		aliasMapInit = true;
		aliasMap["0"]         = "8kB";
		aliasMap["1"]         = "16kB";
		aliasMap["MSXDOS2"]   = "16kB";
		aliasMap["2"]         = "KONAMI5";
		aliasMap["SCC"]       = "KONAMI5";
		aliasMap["3"]         = "KONAMI4";
		aliasMap["4"]         = "ASCII8";
		aliasMap["5"]         = "ASCII16";
		aliasMap["64kB"]      = "PLAIN";
		aliasMap["ASCII16-2"] = "HYDLIDE2";
		aliasMap["RC755"]     = "GAME_MASTER2";
		aliasMap["ROMBAS"]    = "PAGE2";
	}
	map<string, string>::const_iterator alias_it = aliasMap.find(name);
	if (alias_it != aliasMap.end()) {
		name = alias_it->second;
		assert(mappertype.find(name) != mappertype.end());
	}
	MapperTypeMap::const_iterator it = mappertype.find(name);
	if (it == mappertype.end()) {
		return UNKNOWN;
	}
	return it->second;
}

void RomInfo::getAllMapperTypes(set<string>& result)
{
	initMap();
	for (MapperTypeMap::const_iterator it = mappertype.begin();
	     it != mappertype.end(); ++it) {
		result.insert(it->first);
	}
}

static void parseDB(const XMLDocument& doc, map<string, RomInfo*>& result)
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
		   company, remark, RomInfo::nameToMapperType(romtype));
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
				XMLDocument doc(file.getLocalName(), "romdb.dtd");
				map<string, RomInfo*> tmp;
				parseDB(doc, tmp);
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

