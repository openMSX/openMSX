// $Id$

#include <map>
#include <string>
#include "RomInfo.hh"
#include "Rom.hh"
#include "xmlx.hh"
#include "FileContext.hh"
#include "File.hh"
#include "CliCommOutput.hh"
#include "StringOp.hh"

namespace openmsx {

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

// TODO: Turn MapperType into a class and move naming there.
MapperType RomInfo::nameToMapperType(const string& name)
{
	typedef map<string, MapperType, StringOp::caseless> MapperTypeMap;
	static MapperTypeMap mappertype;
	static bool init = false;

	if (!init) {
		init = true;
		
		mappertype["0"]           = GENERIC_8KB;
		mappertype["8kB"]         = GENERIC_8KB;

		mappertype["1"]           = GENERIC_16KB;
		mappertype["16kB"]        = GENERIC_16KB;
		mappertype["MSXDOS2"]     = GENERIC_16KB; /* For now...*/

		mappertype["2"]           = KONAMI5;
		mappertype["KONAMI5"]     = KONAMI5;
		mappertype["SCC"]         = KONAMI5;

		mappertype["3"]           = KONAMI4;
		mappertype["KONAMI4"]     = KONAMI4;

		mappertype["4"]           = ASCII_8KB;
		mappertype["ASCII8"]      = ASCII_8KB;
 
		mappertype["5"]           = ASCII_16KB;
		mappertype["ASCII16"]     = ASCII_16KB;

		mappertype["RTYPE"]       = R_TYPE;

		mappertype["CROSSBLAIM"]  = CROSS_BLAIM;
		
		mappertype["PANASONIC"]   = PANASONIC;
		
		mappertype["NATIONAL"]    = NATIONAL;
	
		mappertype["MSX-AUDIO"]   = MSX_AUDIO;
		
		mappertype["HARRYFOX"]    = HARRY_FOX;
		
		mappertype["HALNOTE"]     = HALNOTE;
		
		mappertype["KOREAN80IN1"] = KOREAN80IN1;
		
		mappertype["KOREAN90IN1"] = KOREAN90IN1;
		
		mappertype["KOREAN126IN1"]= KOREAN126IN1;
		
		mappertype["HOLYQURAN"]   = HOLY_QURAN;
	
		mappertype["FSA1FM1"]     = FSA1FM1;
		mappertype["FSA1FM2"]     = FSA1FM2;
		
		mappertype["64kB"]        = PLAIN;
		mappertype["PLAIN"]       = PLAIN;
		
		// SRAM
		mappertype["HYDLIDE2"]    = HYDLIDE2;
		mappertype["ASCII16-2"]   = HYDLIDE2;

		mappertype["ASCII8-8"]    = ASCII8_8; 
		mappertype["KOEI-8"]      = KOEI_8; 
		mappertype["KOEI-32"]     = KOEI_32; 
		mappertype["WIZARDRY"]    = WIZARDRY; 

		mappertype["GAMEMASTER2"] = GAME_MASTER2;
		mappertype["RC755"]       = GAME_MASTER2;

		// DAC
		mappertype["MAJUTSUSHI"]  = MAJUTSUSHI;
		
		mappertype["SYNTHESIZER"] = SYNTHESIZER;
		
		mappertype["PAGE0"]       = PAGE0;
		mappertype["PAGE01"]      = PAGE01;
		mappertype["PAGE012"]     = PAGE012;
		mappertype["PAGE0123"]    = PAGE0123;
		mappertype["PAGE1"]       = PAGE1;
		mappertype["PAGE12"]      = PAGE12;
		mappertype["PAGE123"]     = PAGE123;
		mappertype["PAGE2"]       = PAGE2;
		mappertype["ROMBAS"]      = PAGE2;
		mappertype["PAGE23"]      = PAGE23;
		mappertype["PAGE3"]       = PAGE3;
	}

	MapperTypeMap::const_iterator it = mappertype.find(name);
	if (it == mappertype.end()) {
		return UNKNOWN;
	}
	return it->second;
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

