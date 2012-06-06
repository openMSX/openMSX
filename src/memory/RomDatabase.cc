// $Id$

#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "Rom.hh"
#include "InfoTopic.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "FileContext.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "LocalFileReference.hh"
#include "GlobalCommandController.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include <libxml/parser.h>
#include <libxml/xmlversion.h>
#include <set>
#include <cassert>
#include <cstring>

using std::auto_ptr;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

class SoftwareInfoTopic : public InfoTopic
{
public:
        SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, RomDatabase& romDatabase);

        virtual void execute(const std::vector<TclObject*>& tokens,
                             TclObject& result) const;
        virtual std::string help(const std::vector<std::string>& tokens) const;
        virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	const RomDatabase& romDatabase;
};



typedef std::map<std::string, unsigned> UnknownTypes;

struct DBParser
{
	DBParser(RomDatabase::DBMap& romDBSHA1_, UnknownTypes& unknownTypes_,
	         CliComm& cliComm_)
		: romDBSHA1(romDBSHA1_)
		, unknownTypes(unknownTypes_)
		, cliComm(cliComm_)
		, state(BEGIN)
		, unknownLevel(0)
	{
	}

	enum State {
		BEGIN,
		SOFTWAREDB,
		SOFTWARE,
		SYSTEM,
		TITLE,
		COMPANY,
		YEAR,
		COUNTRY,
		GENMSXID,
		SW_REMARK,
		SW_TEXT,
		DUMP_REMARK,
		DUMP_TEXT,
		DUMP,
		ORIGINAL,
		ROM,
		TYPE,
		START,
		HASH,
		END
	};

	struct Dump {
		string remarks;
		Sha1Sum hash;
		string origData;
		RomType type;
		bool origValue;
	};

	RomDatabase::DBMap& romDBSHA1;
	UnknownTypes& unknownTypes;
	CliComm& cliComm;
	set<Sha1Sum> sums;

	string systemID;
	string data;
	string type;
	string start;
	string algo;

	vector<Dump> dumps;
	string system;
	string title;
	string company;
	string year;
	string country;
	string remarks;
	int genMSXid;

	State state;
	unsigned unknownLevel;
};

void copyTrimmed(const string& input, string& output)
{
	string::size_type begin = input.find_first_not_of(" \t\n\r");
	if (begin == string::npos) {
		output.clear();
		return;
	}
	string::size_type end = input.find_last_not_of(" \t\n\r");
	assert(end != string::npos);
	output.assign(input, begin, end - begin + 1);
}
static string trim(const string& s)
{
	string result;
	copyTrimmed(s, result);
	return result;
}


static void joinRemarks(string& result, const string& extra)
{
	if (extra.empty()) return;
	if (!result.empty()) result += '\n';
	result += extra;
}

static string getAttribute(const char* wantedAttribute,
                           int nb_attributes, const xmlChar** attrs)
{
	for (int i = 0; i < nb_attributes; i++) {
		if (strcmp(reinterpret_cast<const char*>(attrs[i * 5 + 0]),
		           wantedAttribute) == 0) {
			const char* valueStart =
				reinterpret_cast<const char*>(attrs[i * 5 + 3]);
			const char* valueEnd =
				reinterpret_cast<const char*>(attrs[i * 5 + 4]);
			return string(valueStart, valueEnd - valueStart);
		}
	}
	return "";
}

static void cbStartElement(
	DBParser* parser,
	const xmlChar* localname, const xmlChar* /*prefix*/, const xmlChar* /*uri*/,
	int /*nb_namespaces*/, const xmlChar** /*namespaces*/,
	int nb_attributes, int /*nb_defaulted*/, const xmlChar** attrs)
{
	parser->data.clear();
	if (parser->unknownLevel) {
		++parser->unknownLevel;
		return;
	}

	const char* tag = reinterpret_cast<const char*>(localname);
	switch (parser->state) {
	case DBParser::BEGIN:
		if (strcmp(tag, "softwaredb") == 0) {
			parser->state = DBParser::SOFTWAREDB;
		} else {
			throw MSXException("Expected <softwaredb> as root tag.");
		}
		break;
	case DBParser::SOFTWAREDB:
		if (strcmp(tag, "software") == 0) {
			parser->system.clear();
			parser->title.clear();
			parser->company.clear();
			parser->year.clear();
			parser->country.clear();
			parser->remarks.clear();
			parser->genMSXid = 0;
			parser->dumps.clear();
			parser->state = DBParser::SOFTWARE;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::SOFTWARE:
		if        (strcmp(tag, "system") == 0) {
			parser->state = DBParser::SYSTEM;
		} else if (strcmp(tag, "title") == 0) {
			parser->state = DBParser::TITLE;
		} else if (strcmp(tag, "company") == 0) {
			parser->state = DBParser::COMPANY;
		} else if (strcmp(tag, "year") == 0) {
			parser->state = DBParser::YEAR;
		} else if (strcmp(tag, "country") == 0) {
			parser->state = DBParser::COUNTRY;
		} else if (strcmp(tag, "remark") == 0) {
			parser->state = DBParser::SW_REMARK;
		} else if (strcmp(tag, "genmsxid") == 0) {
			parser->state = DBParser::GENMSXID;
		} else if (strcmp(tag, "dump") == 0) {
			parser->dumps.resize(parser->dumps.size() + 1);
			parser->dumps.back().type = ROM_UNKNOWN;
			parser->dumps.back().origValue = false;
			parser->state = DBParser::DUMP;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::DUMP:
		if        (strcmp(tag, "original") == 0) {
			parser->dumps.back().origValue = StringOp::stringToBool(
				getAttribute("value", nb_attributes, attrs));
			parser->state = DBParser::ORIGINAL;
		} else if (strcmp(tag, "megarom") == 0) {
			parser->type.clear();
			parser->start.clear();
			parser->state = DBParser::ROM;
		} else if (strcmp(tag, "rom") == 0) {
			parser->type = "Mirrored";
			parser->start.clear();
			parser->state = DBParser::ROM;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::ROM:
		if        (strcmp(tag, "type") == 0) {
			parser->state = DBParser::TYPE;
		} else if (strcmp(tag, "start") == 0) {
			parser->state = DBParser::START;
		} else if (strcmp(tag, "remark") == 0) {
			parser->state = DBParser::DUMP_REMARK;
		} else if (strcmp(tag, "hash") == 0) {
			parser->algo = getAttribute("algo", nb_attributes, attrs);
			parser->state = DBParser::HASH;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::SW_REMARK:
		if (strcmp(tag, "text") == 0) {
			parser->state = DBParser::SW_TEXT;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::DUMP_REMARK:
		if (strcmp(tag, "text") == 0) {
			parser->state = DBParser::DUMP_TEXT;
		} else {
			++parser->unknownLevel;
		}
		break;
	case DBParser::SYSTEM:
	case DBParser::TITLE:
	case DBParser::COMPANY:
	case DBParser::YEAR:
	case DBParser::COUNTRY:
	case DBParser::GENMSXID:
	case DBParser::ORIGINAL:
	case DBParser::TYPE:
	case DBParser::START:
	case DBParser::HASH:
	case DBParser::SW_TEXT:
	case DBParser::DUMP_TEXT:
		++parser->unknownLevel;
		break;

	case DBParser::END:
		throw MSXException("Unexpected opening tag: " + string(tag));

	default:
		assert(false);
	}
}

// called on </software>
static void addEntries(DBParser* parser)
{
	if (!parser->system.empty() && (parser->system != "MSX")) {
		// skip non-MSX entries
		return;
	}

	for (vector<DBParser::Dump>::const_iterator it = parser->dumps.begin();
	     it != parser->dumps.end(); ++it) {
		if (!parser->sums.insert(it->hash).second) {
			parser->cliComm.printWarning(
				"duplicate softwaredb entry SHA1: " +
				it->hash.toString());
			continue;
		}

		RomInfo*& ptr = parser->romDBSHA1[it->hash];
		if (ptr) {
			// User database already had this entry, don't overwrite
			// with the value from the system database.
			return;
		}

		string remarks = parser->remarks;
		joinRemarks(remarks, it->remarks);

		ptr = new RomInfo(
			parser->title, parser->year, parser->company, parser->country,
			it->origValue, it->origData, remarks, it->type,
			parser->genMSXid);
	}
}

static string parseStart(const string& start)
{
	if      (start == "0x0000") return "0000";
	else if (start == "0x4000") return "4000";
	else if (start == "0x8000") return "8000";
	else if (start == "0xC000") return "C000";
	else return "";
}

static void cbEndElement(
	DBParser* parser,
	const xmlChar* /*localname*/, const xmlChar* /*prefix*/, const xmlChar* /*uri*/)
{
	if (parser->unknownLevel) {
		--parser->unknownLevel;
		parser->data.clear();
		return;
	}

	switch (parser->state) {
	case DBParser::SOFTWAREDB:
		parser->state = DBParser::END;
		break;
	case DBParser::SOFTWARE:
		addEntries(parser);
		parser->state = DBParser::SOFTWAREDB;
		break;
	case DBParser::SYSTEM:
		copyTrimmed(parser->data, parser->system);
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::TITLE:
		copyTrimmed(parser->data, parser->title);
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::COMPANY:
		copyTrimmed(parser->data, parser->company);
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::YEAR:
		copyTrimmed(parser->data, parser->year);
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::COUNTRY:
		copyTrimmed(parser->data, parser->country);
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::GENMSXID:
		if (!StringOp::stringToInt(trim(parser->data), parser->genMSXid)) {
			parser->cliComm.printWarning(StringOp::Builder() <<
				"Ignoring bad Generation MSX id (genmsxid) "
				"in entry with title '" << parser->title <<
				": " << parser->data);
		}
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::SW_REMARK:
		joinRemarks(parser->remarks, trim(parser->data));
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::SW_TEXT:
		joinRemarks(parser->remarks, trim(parser->data));
		parser->state = DBParser::SW_REMARK;
		break;
	case DBParser::DUMP_REMARK:
		joinRemarks(parser->dumps.back().remarks, trim(parser->data));
		parser->state = DBParser::ROM;
		break;
	case DBParser::DUMP_TEXT:
		joinRemarks(parser->dumps.back().remarks, trim(parser->data));
		parser->state = DBParser::DUMP_REMARK;
		break;
	case DBParser::DUMP:
		if (parser->dumps.back().hash.empty()) {
			// no sha1 sum specified, drop this dump
			parser->dumps.pop_back();
		}
		parser->state = DBParser::SOFTWARE;
		break;
	case DBParser::ORIGINAL:
		copyTrimmed(parser->data, parser->dumps.back().origData);
		parser->state = DBParser::DUMP;
		break;
	case DBParser::ROM: {
		string type = parser->type;
		if ((type == "Mirrored") || (type == "Normal")) {
			type += parseStart(parser->start);
		}
		RomType romType = RomInfo::nameToRomType(type);
		if (romType == ROM_UNKNOWN) {
			parser->unknownTypes[type]++;
		}
		parser->dumps.back().type = romType;
		parser->state = DBParser::DUMP;
		break;
	}
	case DBParser::TYPE:
		copyTrimmed(parser->data, parser->type);
		parser->state = DBParser::ROM;
		break;
	case DBParser::START:
		copyTrimmed(parser->data, parser->start);
		parser->state = DBParser::ROM;
		break;
	case DBParser::HASH:
		if (parser->algo == "sha1") {
			string sha1 = trim(parser->data);
			try {
				parser->dumps.back().hash = Sha1Sum(sha1);
			} catch (MSXException& e) {
				parser->cliComm.printWarning(StringOp::Builder() <<
					"Ignoring entry with bad sha1: " <<
					parser->title << ": " << e.getMessage());
			}
		}
		parser->state = DBParser::ROM;
		break;
	case DBParser::BEGIN:
	case DBParser::END:
		throw MSXException("Unexpected closing tag");

	default:
		assert(false);
	}

	parser->data.clear();
}

static void cbCharacters(DBParser* parser, const xmlChar* chars, int len)
{
	parser->data.append(reinterpret_cast<const char*>(chars), len);
}

static void cbInternalSubset(DBParser* parser, const xmlChar* /*name*/,
                             const xmlChar* /*extID*/, const xmlChar* systemID)
{
	parser->systemID = reinterpret_cast<const char*>(systemID);
}

static void parseDB(CliComm& cliComm, const string& filename,
                    RomDatabase::DBMap& romDBSHA1, UnknownTypes& unknownTypes)
{
	File file(filename);
	unsigned size;
	const char* buffer = reinterpret_cast<const char*>(file.mmap(size));

	xmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.startElementNs = (startElementNsSAX2Func)cbStartElement;
	handler.endElementNs   = (endElementNsSAX2Func)  cbEndElement;
	handler.characters     = (charactersSAXFunc)     cbCharacters;
	handler.internalSubset = (internalSubsetSAXFunc) cbInternalSubset;
	handler.initialized = XML_SAX2_MAGIC;

	DBParser parser(romDBSHA1, unknownTypes, cliComm);

	xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(
		&handler, &parser, NULL, 0, filename.c_str());
	if (!ctxt) {
		throw MSXException("Could not create XML parser context");
	}
	int parseError = xmlParseChunk(ctxt, buffer, size, true);
	xmlFreeParserCtxt(ctxt);

	if (parseError) {
		throw MSXException("Document parsing failed");
	}
	if (parser.systemID.empty()) {
		throw MSXException(
			"Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	if (parser.systemID != "softwaredb1.dtd") {
		throw MSXException(
			"systemID doesn't match "
			"(expected softwaredb1.dtd, got " + parser.systemID + ")\n"
			"You're probably using an old incompatible file format.");
	}
}

RomDatabase::RomDatabase(GlobalCommandController& commandController, CliComm& cliComm)
	: softwareInfoTopic(new SoftwareInfoTopic(
		commandController.getOpenMSXInfoCommand(), *this))
{
	UnknownTypes unknownTypes;
	SystemFileContext context; // first user- then system-directory
	vector<string> paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		string filename = FileOperations::join(*it, "softwaredb.xml");
		try {
			parseDB(cliComm, filename, romDBSHA1, unknownTypes);
		} catch (MSXException& /*e*/) {
			// Ignore. It's not unusual the DB in the user
			// directory is not found. In case there's an error
			// with both user and system DB, we must give a
			// warning, but that's done below.
		}
	}
	if (romDBSHA1.empty()) {
		cliComm.printWarning(
			"Couldn't load software database.\n"
			"This may cause incorrect ROM mapper types to be used.");
	}
	if (!unknownTypes.empty()) {
		StringOp::Builder output;
		output << "Unknown mapper types in software database: ";
		for (UnknownTypes::iterator it = unknownTypes.begin();
		     it != unknownTypes.end(); ++it) {
			output << it->first << " (" << it->second << "x); ";
		}
		cliComm.printWarning(output);
	}
}

RomDatabase::~RomDatabase()
{
	for (DBMap::const_iterator it = romDBSHA1.begin();
	     it != romDBSHA1.end(); ++it) {
		delete it->second;
	}
}

const RomInfo* RomDatabase::fetchRomInfo(const Sha1Sum& sha1sum) const
{
	DBMap::const_iterator it = romDBSHA1.find(sha1sum);
	if (it == romDBSHA1.end()) {
		return NULL;
	}
	return it->second;
}


// SoftwareInfoTopic

SoftwareInfoTopic::SoftwareInfoTopic(InfoCommand& openMSXInfoCommand,
                                     RomDatabase& romDatabase_)
	: InfoTopic(openMSXInfoCommand, "software")
	, romDatabase(romDatabase_)
{
}

void SoftwareInfoTopic::execute(const vector<TclObject*>& tokens,
                                TclObject& result) const
{
	if (tokens.size() != 3) {
		throw CommandException("Wrong number of parameters");
	}

	Sha1Sum sha1sum = Sha1Sum(tokens[2]->getString());
	const RomInfo* romInfo = romDatabase.fetchRomInfo(sha1sum);
	if (romInfo == NULL) {
		// no match found
		throw CommandException(
			"Software with sha1sum " + sha1sum.toString() + " not found");
	}

	result.addListElement("title");
	result.addListElement(romInfo->getTitle());
	result.addListElement("year");
	result.addListElement(romInfo->getYear());
	result.addListElement("company");
	result.addListElement(romInfo->getCompany());
	result.addListElement("country");
	result.addListElement(romInfo->getCountry());
	result.addListElement("orig_type");
	result.addListElement(romInfo->getOrigType());
	result.addListElement("remark");
	result.addListElement(romInfo->getRemark());
	result.addListElement("original");
	result.addListElement(romInfo->getOriginal());
	result.addListElement("mapper_type_name");
	result.addListElement(RomInfo::romTypeToName(romInfo->getRomType()));
	result.addListElement("genmsxid");
	result.addListElement(romInfo->getGenMSXid());
}

string SoftwareInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Returns information about the software "
	       "given its sha1sum, in a paired list.";
}

void SoftwareInfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// no useful completion possible
}

} // namespace openmsx
