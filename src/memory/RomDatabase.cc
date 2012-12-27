// $Id$

#include "RomDatabase.hh"
#include "RomInfo.hh"
#include "InfoTopic.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "FileContext.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "GlobalCommandController.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "MemBuffer.hh"
#include "StringMap.hh"
#include "rapidsax.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <set>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class SoftwareInfoTopic : public InfoTopic
{
public:
        SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, RomDatabase& romDatabase);

        virtual void execute(const vector<TclObject>& tokens,
                             TclObject& result) const;
        virtual string help(const vector<string>& tokens) const;
        virtual void tabCompletion(vector<string>& tokens) const;

private:
	const RomDatabase& romDatabase;
};



typedef StringMap<unsigned> UnknownTypes;

class DBParser : public rapidsax::NullHandler
{
public:
	DBParser(RomDatabase::DBMap& romDBSHA1_, UnknownTypes& unknownTypes_,
	         CliComm& cliComm_)
		: romDBSHA1(romDBSHA1_)
		, unknownTypes(unknownTypes_)
		, cliComm(cliComm_)
		, state(BEGIN)
		, unknownLevel(0)
	{
	}

	// rapidsax handler interface
	void start(string_ref name);
	void attribute(string_ref name, string_ref value);
	void text(string_ref text);
	void stop();
	void doctype(string_ref text);

	string_ref getSystemID() const { return systemID; }

private:
	void addEntries();

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
		string_ref origData;
		RomType type;
		bool origValue;
	};

	RomDatabase::DBMap& romDBSHA1;
	UnknownTypes& unknownTypes;
	CliComm& cliComm;
	set<Sha1Sum> sums;

	string_ref systemID;
	string_ref type;
	string_ref startVal;
	string_ref algo;

	vector<Dump> dumps;
	string_ref system;
	string_ref title;
	string_ref company;
	string_ref year;
	string_ref country;
	string remarks;
	int genMSXid;

	State state;
	unsigned unknownLevel;
};

void DBParser::start(string_ref tag)
{
	if (unknownLevel) {
		++unknownLevel;
		return;
	}

	switch (state) {
	case BEGIN:
		if (tag == "softwaredb") {
			state = SOFTWAREDB;
		} else {
			throw MSXException("Expected <softwaredb> as root tag.");
		}
		break;
	case SOFTWAREDB:
		if (tag == "software") {
			system.clear();
			title.clear();
			company.clear();
			year.clear();
			country.clear();
			remarks.clear();
			genMSXid = 0;
			dumps.clear();
			state = SOFTWARE;
		} else {
			++unknownLevel;
		}
		break;
	case SOFTWARE:
		if        (tag == "system") {
			state = SYSTEM;
		} else if (tag == "title") {
			state = TITLE;
		} else if (tag == "company") {
			state = COMPANY;
		} else if (tag == "year") {
			state = YEAR;
		} else if (tag == "country") {
			state = COUNTRY;
		} else if (tag == "remark") {
			state = SW_REMARK;
		} else if (tag == "genmsxid") {
			state = GENMSXID;
		} else if (tag == "dump") {
			dumps.resize(dumps.size() + 1);
			dumps.back().type = ROM_UNKNOWN;
			dumps.back().origValue = false;
			state = DUMP;
		} else {
			++unknownLevel;
		}
		break;
	case DUMP:
		if        (tag == "original") {
			dumps.back().origValue = false;
			state = ORIGINAL;
		} else if (tag == "megarom") {
			type.clear();
			startVal.clear();
			state = ROM;
		} else if (tag == "rom") {
			type = "Mirrored";
			startVal.clear();
			state = ROM;
		} else {
			++unknownLevel;
		}
		break;
	case ROM:
		if        (tag == "type") {
			state = TYPE;
		} else if (tag == "start") {
			state = START;
		} else if (tag == "remark") {
			state = DUMP_REMARK;
		} else if (tag == "hash") {
			algo.clear();
			state = HASH;
		} else {
			++unknownLevel;
		}
		break;
	case SW_REMARK:
		if (tag == "text") {
			state = SW_TEXT;
		} else {
			++unknownLevel;
		}
		break;
	case DUMP_REMARK:
		if (tag == "text") {
			state = DUMP_TEXT;
		} else {
			++unknownLevel;
		}
		break;
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
	case ORIGINAL:
	case TYPE:
	case START:
	case HASH:
	case SW_TEXT:
	case DUMP_TEXT:
		++unknownLevel;
		break;

	case END:
		throw MSXException("Unexpected opening tag: " + tag);

	default:
		UNREACHABLE;
	}
}

void DBParser::attribute(string_ref name, string_ref value)
{
	if (unknownLevel) return;

	switch (state) {
	case ORIGINAL:
		if (name == "value") {
			dumps.back().origValue = StringOp::stringToBool(value);
		}
		break;
	case HASH:
		if (name == "algo") {
			algo = value;
		}
		break;
	case BEGIN:
	case SOFTWAREDB:
	case SOFTWARE:
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
	case SW_REMARK:
	case SW_TEXT:
	case DUMP_REMARK:
	case DUMP_TEXT:
	case DUMP:
	case ROM:
	case TYPE:
	case START:
	case END:
		break;
	default:
		UNREACHABLE;
	}
}

static void joinRemarks(string& result, string_ref extra)
{
	if (extra.empty()) return;
	if (!result.empty()) result += '\n';
	result.append(extra.data(), extra.size());
}

void DBParser::text(string_ref text)
{
	if (unknownLevel) return;

	switch (state) {
	case SYSTEM:
		system = text;
		break;
	case TITLE:
		title = text;
		break;
	case COMPANY:
		company = text;
		break;
	case YEAR:
		year = text;
		break;
	case COUNTRY:
		country = text;
		break;
	case GENMSXID:
		genMSXid = stoi(text);
		// TODO error checks?
		//	cliComm.printWarning(StringOp::Builder() <<
		//		"Ignoring bad Generation MSX id (genmsxid) "
		//		"in entry with title '" << title <<
		//		": " << data);
		break;
	case ORIGINAL:
		dumps.back().origData = text;
		break;
	case TYPE:
		type = text;
		break;
	case START:
		startVal = text;
		break;
	case HASH:
		if (algo == "sha1") {
			dumps.back().hash = Sha1Sum(text);
		}
		break;
	case SW_REMARK:
	case SW_TEXT:
		joinRemarks(remarks, text);
		break;
	case DUMP_REMARK:
	case DUMP_TEXT:
		joinRemarks(dumps.back().remarks, text);
		break;
	case BEGIN:
	case SOFTWAREDB:
	case SOFTWARE:
	case DUMP:
	case ROM:
	case END:
		break;
	default:
		UNREACHABLE;
	}
}

// called on </software>
void DBParser::addEntries()
{
	if (!system.empty() && (system != "MSX")) {
		// skip non-MSX entries
		return;
	}

	for (auto& d : dumps) {
		if (!sums.insert(d.hash).second) {
			cliComm.printWarning(
				"duplicate softwaredb entry SHA1: " +
				d.hash.toString());
			continue;
		}

		auto& ptr = romDBSHA1[d.hash];
		if (ptr.get()) {
			// User database already had this entry, don't overwrite
			// with the value from the system database.
			continue;
		}

		string r = remarks;
		joinRemarks(r, d.remarks);

		ptr = make_unique<RomInfo>(
			title, year, company, country,
			d.origValue, d.origData, r, d.type,
			genMSXid);
	}
}

static const char* parseStart(string_ref s)
{
	// we expect "0x0000", "0x4000", "0x8000", "0xc000" or ""
	return ((s.size() == 6) && s.starts_with("0x")) ? (s.data() + 2) : nullptr;
}

void DBParser::stop()
{
	if (unknownLevel) {
		--unknownLevel;
		return;
	}

	switch (state) {
	case SOFTWAREDB:
		state = END;
		break;
	case SOFTWARE:
		addEntries();
		state = SOFTWAREDB;
		break;
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
	case SW_REMARK:
		state = SOFTWARE;
		break;
	case SW_TEXT:
		state = SW_REMARK;
		break;
	case DUMP:
		if (dumps.back().hash.empty()) {
			// no sha1 sum specified, drop this dump
			dumps.pop_back();
		}
		state = SOFTWARE;
		break;
	case ORIGINAL:
		state = DUMP;
		break;
	case ROM: {
		string_ref t = type;
		char buf[12];
		if (t == "Mirrored") {
			if (const char* start = parseStart(startVal)) {
				memcpy(buf, t.data(), 8);
				memcpy(buf + 8, start, 4);
				t = string_ref(buf, 12);
			}
		} else if (t == "Normal") {
			if (const char* start = parseStart(startVal)) {
				memcpy(buf, t.data(), 6);
				memcpy(buf + 6, start, 4);
				t = string_ref(buf, 10);
			}
		}
		RomType romType = RomInfo::nameToRomType(t);
		if (romType == ROM_UNKNOWN) {
			unknownTypes[t]++;
		}
		dumps.back().type = romType;
		state = DUMP;
		break;
	}
	case TYPE:
	case START:
	case HASH:
	case DUMP_REMARK:
		state = ROM;
		break;
	case DUMP_TEXT:
		state = DUMP_REMARK;
		break;
	case BEGIN:
	case END:
		throw MSXException("Unexpected closing tag");

	default:
		UNREACHABLE;
	}
}

void DBParser::doctype(string_ref text)
{
	auto pos1 = text.find(" SYSTEM \"");
	if (pos1 == string_ref::npos) return;
	auto t = text.substr(pos1 + 9);
	auto pos2 = t.find('"');
	if (pos2 == string_ref::npos) return;
	systemID = t.substr(0, pos2);
}

static void parseDB(CliComm& cliComm, const string& filename,
                    RomDatabase::DBMap& romDBSHA1, UnknownTypes& unknownTypes)
{
	File file(filename);
	unsigned size = file.getSize();
	MemBuffer<char> buf(size + 1);
	file.read(buf.data(), size);
	buf[size] = 0;

	DBParser handler(romDBSHA1, unknownTypes, cliComm);
	rapidsax::parse<rapidsax::trimWhitespace>(handler, buf.data());

	if (handler.getSystemID() != "softwaredb1.dtd") {
		throw rapidsax::ParseError(
			"Missing or wrong systemID.\n"
			"You're probably using an old incompatible file format.",
			nullptr);
	}
}

RomDatabase::RomDatabase(GlobalCommandController& commandController, CliComm& cliComm)
	: softwareInfoTopic(make_unique<SoftwareInfoTopic>(
		commandController.getOpenMSXInfoCommand(), *this))
{
	UnknownTypes unknownTypes;
	// first user- then system-directory
	vector<string> paths = SystemFileContext().getPaths();
	for (auto& p : paths) {
		string filename = FileOperations::join(p, "softwaredb.xml");
		try {
			parseDB(cliComm, filename, romDBSHA1, unknownTypes);
		} catch (rapidsax::ParseError& e) {
			cliComm.printWarning(StringOp::Builder() <<
				"Rom database parsing failed: " << e.what());
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
		for (auto& p : unknownTypes) {
			output << p.first() << " (" << p.second << "x); ";
		}
		cliComm.printWarning(output);
	}
}

RomDatabase::~RomDatabase()
{
}

const RomInfo* RomDatabase::fetchRomInfo(const Sha1Sum& sha1sum) const
{
	auto it = romDBSHA1.find(sha1sum);
	if (it == romDBSHA1.end()) {
		return nullptr;
	}
	return it->second.get();
}


// SoftwareInfoTopic

SoftwareInfoTopic::SoftwareInfoTopic(InfoCommand& openMSXInfoCommand,
                                     RomDatabase& romDatabase_)
	: InfoTopic(openMSXInfoCommand, "software")
	, romDatabase(romDatabase_)
{
}

void SoftwareInfoTopic::execute(const vector<TclObject>& tokens,
                                TclObject& result) const
{
	if (tokens.size() != 3) {
		throw CommandException("Wrong number of parameters");
	}

	Sha1Sum sha1sum = Sha1Sum(tokens[2].getString());
	const RomInfo* romInfo = romDatabase.fetchRomInfo(sha1sum);
	if (romInfo == nullptr) {
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
