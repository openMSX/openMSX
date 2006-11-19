// $Id$

#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "Rom.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "File.hh"
#include "GlobalCliComm.hh"
#include "StringOp.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include <map>
#include <cassert>

using std::auto_ptr;
using std::map;
using std::set;
using std::string;
using std::vector;

// Note: Using something like boost::shared_ptr would simplify memory management
//       but it does add another dependency. So atm we don't use it yet.

namespace openmsx {

typedef map<string, RomInfo*, StringOp::caseless> DBMap;
static DBMap romDBSHA1;


RomDatabase::RomDatabase()
{
}

RomDatabase::~RomDatabase()
{
	for (DBMap::const_iterator it = romDBSHA1.begin();
	     it != romDBSHA1.end(); ++it) {
		delete it->second;
	}
}

RomDatabase& RomDatabase::instance()
{
	static RomDatabase oneInstance;
	return oneInstance;
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

static void addEntry(GlobalCliComm& cliComm, auto_ptr<RomInfo> romInfo,
                     const string& sha1, DBMap& result)
{
	assert(romInfo.get());
	if (result.find(sha1) == result.end()) {
		result[sha1] = romInfo.release();
	} else {
		cliComm.printWarning(
			"duplicate softwaredb entry SHA1: " + sha1);
	}
}

static void parseEntry(GlobalCliComm& cliComm,
	const XMLElement& rom, DBMap& result,
	const string& title,   const string& year,
	const string& company, const string& country,
	bool original,         const string& origType,
	const string& remark,  const string& type)
{
	XMLElement::Children hashTags;
	rom.getChildren("hash", hashTags);
	for (XMLElement::Children::const_iterator it2 = hashTags.begin();
	     it2 != hashTags.end(); ++it2) {
		if ((*it2)->getAttribute("algo") != "sha1") {
			continue;
		}
		auto_ptr<RomInfo> romInfo(new RomInfo(
			title, year, company, country, original, origType,
			remark, RomInfo::nameToRomType(type)));
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

static void parseDB(GlobalCliComm& cliComm, const XMLElement& doc, DBMap& result)
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
		int dumpcounter = 0;
		for (XMLElement::Children::const_iterator it2 = dumps.begin();
		     it2 != dumps.end(); ++it2) {
			dumpcounter++;
			const XMLElement& dump = **it2;
			const XMLElement* originalTag = dump.findChild("original");
			bool original = false;
			if (originalTag) {
				original = originalTag->getAttributeAsBool("value");
			} else {
				cliComm.printWarning("Missing <original> tag in software"
					"database for dump " + StringOp::toString(dumpcounter) +
					" of entry with name \"" + title + "\". "
					"Please fix your database!");
			}
			if (const XMLElement* megarom = dump.findChild("megarom")) {
				parseEntry(cliComm, *megarom, result, title, year,
				           company, country, original,
						   originalTag ? originalTag->getData() : "", remark,
				           megarom->getChildData("type"));
			} else if (const XMLElement* rom = dump.findChild("rom")) {
				string type = rom->getChildData("type", "Mirrored");
				if (type == "Normal") {
					type += parseStart(*rom);
				} else if (type == "Mirrored") {
					type += parseStart(*rom);
				}
				parseEntry(cliComm, *rom, result, title, year,
				           company, country, original,
						   originalTag ? originalTag->getData() : "", 
						   remark, type);
			}
		}
	}
}

static auto_ptr<XMLElement> openDB(GlobalCliComm& cliComm, const string& filename,
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

static void initDatabase(GlobalCliComm& cliComm)
{
	static bool init = false;
	if (init) return;
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
			for (DBMap::const_iterator it = tmp.begin();
			     it != tmp.end(); ++it) {
				if (romDBSHA1.find(it->first) == romDBSHA1.end()) {
					// new entry
					romDBSHA1.insert(*it);
				} else {
					// duplicate entry
					delete it->second;
				}
			}
		}
	}
	if (romDBSHA1.empty()) {
		cliComm.printWarning(
			"Couldn't load rom database.\n"
			"Romtype detection might fail because of this.");
	}
}

auto_ptr<RomInfo> RomDatabase::fetchRomInfo(GlobalCliComm& cliComm, const Rom& rom)
{
	// Note: RomInfo is copied only to make ownership managment easier

	initDatabase(cliComm);

	if (rom.getSize() == 0) {
		return auto_ptr<RomInfo>(
			new RomInfo("", "", "", "", false, "", "Empty ROM",
				ROM_UNKNOWN));
	}

	const string& sha1sum = rom.getSHA1Sum();
	if (romDBSHA1.find(sha1sum) != romDBSHA1.end()) {
		romDBSHA1[sha1sum]->print(cliComm);
		// Return a copy of the DB entry.
		return auto_ptr<RomInfo>(new RomInfo(*romDBSHA1[sha1sum]));
	}

	// no match found
	return auto_ptr<RomInfo>(new RomInfo("", "", "", "", false, "", "", 
		ROM_UNKNOWN)); 
}

} // namespace openmsx
