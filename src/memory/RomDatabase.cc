#include "RomDatabase.hh"
#include "FileContext.hh"
#include "File.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "String32.hh"
#include "hash_map.hh"
#include "ranges.hh"
#include "rapidsax.hh"
#include "unreachable.hh"
#include "stl.hh"
#include "view.hh"
#include "xxhash.hh"
#include <cassert>

using std::string_view;

namespace openmsx {

using UnknownTypes = hash_map<std::string, unsigned, XXHasher>;

class DBParser : public rapidsax::NullHandler
{
public:
	DBParser(RomDatabase::RomDB& db_, UnknownTypes& unknownTypes_,
	         CliComm& cliComm_, char* bufStart_)
		: db(db_)
		, unknownTypes(unknownTypes_)
		, cliComm(cliComm_)
		, bufStart(bufStart_)
		, state(BEGIN)
		, unknownLevel(0)
		, initialSize(db.size())
	{
	}

	// rapidsax handler interface
	void start(string_view tag);
	void attribute(string_view name, string_view value);
	void text(string_view txt);
	void stop();
	void doctype(string_view txt);

	[[nodiscard]] string_view getSystemID() const { return systemID; }

private:
	[[nodiscard]] String32 cIndex(string_view str);
	void addEntries();
	void addAllEntries();

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
		String32 remark;
		Sha1Sum hash;
		String32 origData;
		RomType type;
		bool origValue;
	};

	RomDatabase::RomDB& db;
	UnknownTypes& unknownTypes;
	CliComm& cliComm;
	char* bufStart;

	string_view systemID;
	string_view type;
	string_view startVal;

	std::vector<Dump> dumps;
	string_view system;
	String32 title;
	String32 company;
	String32 year;
	String32 country;
	int genMSXid;

	State state;
	unsigned unknownLevel;
	size_t initialSize;
};

void DBParser::start(string_view tag)
{
	if (unknownLevel) {
		++unknownLevel;
		return;
	}

	assert(!tag.empty()); // rapidsax will reject empty tags
	switch (state) {
	case BEGIN:
		if (tag == "softwaredb") {
			state = SOFTWAREDB;
			return;
		}
		throw MSXException("Expected <softwaredb> as root tag.");
	case SOFTWAREDB:
		if (small_compare<'s','o','f','t','w','a','r','e'>(tag)) {
			system = string_view();
			toString32(bufStart, bufStart, title);
			toString32(bufStart, bufStart, company);
			toString32(bufStart, bufStart, year);
			toString32(bufStart, bufStart, country);
			genMSXid = 0;
			dumps.clear();
			state = SOFTWARE;
			return;
		}
		break;
	case SOFTWARE: {
		switch (tag.front()) {
		case 's':
			if (small_compare<'s','y','s','t','e','m'>(tag)) {
				state = SYSTEM;
				return;
			}
			break;
		case 't':
			tag.remove_prefix(1);
			if (small_compare<'i','t','l','e'>(tag)) {
				state = TITLE;
				return;
			}
			break;
		case 'c':
			if (small_compare<'c','o','m','p','a','n','y'>(tag)) {
				state = COMPANY;
				return;
			} else if (small_compare<'c','o','u','n','t','r','y'>(tag)) {
				state = COUNTRY;
				return;
			}
			break;
		case 'y':
			if (small_compare<'y','e','a','r'>(tag)) {
				state = YEAR;
				return;
			}
			break;
		case 'g':
			if (small_compare<'g','e','n','m','s','x','i','d'>(tag)) {
				state = GENMSXID;
				return;
			}
			break;
		case 'd':
			if (small_compare<'d','u','m','p'>(tag)) {
				dumps.resize(dumps.size() + 1);
				dumps.back().type = ROM_UNKNOWN;
				dumps.back().origValue = false;
				toString32(bufStart, bufStart, dumps.back().remark);
				toString32(bufStart, bufStart, dumps.back().origData);
				state = DUMP;
				return;
			}
			break;
		}
		break;
	}
	case DUMP: {
		switch (tag.front()) {
		case 'o':
			if (small_compare<'o','r','i','g','i','n','a','l'>(tag)) {
				dumps.back().origValue = false;
				state = ORIGINAL;
				return;
			}
			break;
		case 'm':
			if (small_compare<'m','e','g','a','r','o','m'>(tag)) {
				type = string_view();
				startVal = string_view();
				state = ROM;
				return;
			}
			break;
		case 'r':
			tag.remove_prefix(1);
			if (small_compare<'o','m'>(tag)) {
				type = "Mirrored";
				startVal = string_view();
				state = ROM;
				return;
			}
			break;
		}
		break;
	}
	case ROM: {
		switch (tag.front()) {
		case 't':
			if (small_compare<'t','y','p','e'>(tag)) {
				state = TYPE;
				return;
			}
			break;
		case 's':
			tag.remove_prefix(1);
			if (small_compare<'t','a','r','t'>(tag)) {
				state = START;
				return;
			}
			break;
		case 'r':
			if (small_compare<'r','e','m','a','r','k'>(tag)) {
				state = DUMP_REMARK;
				return;
			}
			break;
		case 'h':
			if (small_compare<'h','a','s','h'>(tag)) {
				state = HASH;
				return;
			}
			break;
		}
		break;
	}
	case DUMP_REMARK:
		if (small_compare<'t','e','x','t'>(tag)) {
			state = DUMP_TEXT;
			return;
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
	case DUMP_TEXT:
		break;

	case END:
		throw MSXException("Unexpected opening tag: ", tag);

	default:
		UNREACHABLE;
	}

	++unknownLevel;
}

void DBParser::attribute(string_view name, string_view value)
{
	if (unknownLevel) return;

	switch (state) {
	case ORIGINAL:
		if (small_compare<'v','a','l','u','e'>(name)) {
			dumps.back().origValue = StringOp::stringToBool(value);
		}
		break;
	case HASH:
	case BEGIN:
	case SOFTWAREDB:
	case SOFTWARE:
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
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

void DBParser::text(string_view txt)
{
	if (unknownLevel) return;

	switch (state) {
	case SYSTEM:
		system = txt;
		break;
	case TITLE:
		title = cIndex(txt);
		break;
	case COMPANY:
		company = cIndex(txt);
		break;
	case YEAR:
		year = cIndex(txt);
		break;
	case COUNTRY:
		country = cIndex(txt);
		break;
	case GENMSXID: {
		auto g = StringOp::stringToBase<10, unsigned>(txt);
		if (!g) {
			cliComm.printWarning(
				"Ignoring bad Generation MSX id (genmsxid) "
				"in entry with title '", fromString32(bufStart, title),
				": ", txt);
		}
		genMSXid = *g;
		break;
	}
	case ORIGINAL:
		dumps.back().origData = cIndex(txt);
		break;
	case TYPE:
		type = txt;
		break;
	case START:
		startVal = txt;
		break;
	case HASH:
		try {
			dumps.back().hash = Sha1Sum(txt);
		} catch (MSXException& e) {
			cliComm.printWarning(
				"Ignoring bad dump for '", fromString32(bufStart, title),
				"': ", e.getMessage());
		}
		break;
	case DUMP_REMARK:
	case DUMP_TEXT:
		dumps.back().remark = cIndex(txt);
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

String32 DBParser::cIndex(string_view str)
{
	auto* begin = const_cast<char*>(str.data());
	auto* end = begin + str.size();
	*end = 0;
	String32 result;
	toString32(bufStart, begin, result);
	return result;
}

// called on </software>
void DBParser::addEntries()
{
	append(db, view::transform(dumps, [&](auto& d) {
		return RomDatabase::Entry{
			d.hash,
			RomInfo(title, year, company, country, d.origValue,
			        d.origData, d.remark, d.type, genMSXid)};
	}));
}

// called on </softwaredb>
void DBParser::addAllEntries()
{
	// Calculate boundary between old and new entries.
	//  old: [first, mid)   already sorted, no duplicates
	//  new: [mid, last)    not yet sorted, may have duplicates
	//    there may also be duplicates between old and new
	const auto first = begin(db);
	const auto last  = end  (db);
	const auto mid = first + initialSize;
	if (mid == last) return; // no new entries

	// Sort new entries, old entries are already sorted.
	ranges::sort(mid, last, {}, &RomDatabase::Entry::sha1);

	// Filter duplicates from new entries. This is similar to the
	// unique() algorithm, except that it also warns about duplicates.
	auto it1 = mid;
	auto it2 = mid + 1;
	// skip initial non-duplicates
	while (it2 != last) {
		if (it1->sha1 == it2->sha1) break;
		++it1; ++it2;
	}
	// move non-duplicates up
	while (it2 != last) {
		if (it1->sha1 == it2->sha1) {
			cliComm.printWarning(
				"duplicate softwaredb entry SHA1: ",
				it2->sha1.toString());
		} else {
			++it1;
			*it1 = std::move(*it2);
		}
		++it2;
	}
	// actually erase the duplicates (typically none)
	db.erase(it1 + 1, last);
	// At this point both old and new entries are sorted and unique. But
	// there may still be duplicates between old and new.

	// Merge new and old entries. This is similar to the inplace_merge()
	// algorithm, except that duplicates (between old and new) are removed.
	if (first == mid) return; // no old entries (common case)
	RomDatabase::RomDB result;
	result.reserve(db.size());
	it1 = first;
	it2 = mid;
	// while both new and old still have elements
	while (it1 != mid && it2 != last) {
		if (it1->sha1 < it2->sha1) {
			result.push_back(std::move(*it1));
			++it1;
		} else {
			if (it1->sha1 != it2->sha1) { // *it2 < *it1
				result.push_back(std::move(*it2));
				++it2;
			} else {
				// pick old entry, silently ignore new
				result.push_back(std::move(*it1));
				++it1; ++it2;
			}
		}
	}
	// move remaining old or new entries (one of these is empty)
	move(it1, mid,  back_inserter(result));
	move(it2, last, back_inserter(result));

	// make result the new current database
	swap(result, db);
}

static const char* parseStart(string_view s)
{
	// we expect "0x0000", "0x4000", "0x8000", "0xc000" or ""
	return ((s.size() == 6) && StringOp::startsWith(s, "0x")) ? (s.data() + 2) : nullptr;
}

void DBParser::stop()
{
	if (unknownLevel) {
		--unknownLevel;
		return;
	}

	switch (state) {
	case SOFTWAREDB:
		addAllEntries();
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
		state = SOFTWARE;
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
		string_view t = type;
		char buf[12];
		if (small_compare<'M','i','r','r','o','r','e','d'>(t)) {
			if (const char* s = parseStart(startVal)) {
				memcpy(buf, t.data(), 8);
				memcpy(buf + 8, s, 4);
				t = string_view(buf, 12);
			}
		} else if (small_compare<'N','o','r','m','a','l'>(t)) {
			if (const char* s = parseStart(startVal)) {
				memcpy(buf, t.data(), 6);
				memcpy(buf + 6, s, 4);
				t = string_view(buf, 10);
			}
		}
		RomType romType = RomInfo::nameToRomType(t);
		if (romType == ROM_UNKNOWN) {
			unknownTypes[std::string(t)]++;
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

void DBParser::doctype(string_view txt)
{
	auto pos1 = txt.find(" SYSTEM \"");
	if (pos1 == string_view::npos) return;
	auto t = txt.substr(pos1 + 9);
	auto pos2 = t.find('"');
	if (pos2 == string_view::npos) return;
	systemID = t.substr(0, pos2);
}

static void parseDB(CliComm& cliComm, char* buf, char* bufStart,
                    RomDatabase::RomDB& db, UnknownTypes& unknownTypes)
{
	DBParser handler(db, unknownTypes, cliComm, bufStart);
	rapidsax::parse<rapidsax::trimWhitespace>(handler, buf);

	if (handler.getSystemID() != "softwaredb1.dtd") {
		throw rapidsax::ParseError(
			"Missing or wrong systemID.\n"
			"You're probably using an old incompatible file format.",
			nullptr);
	}
}

RomDatabase::RomDatabase(CliComm& cliComm)
{
	db.reserve(3500);
	UnknownTypes unknownTypes;
	// first user- then system-directory
	std::vector<std::string> paths = systemFileContext().getPaths();
	std::vector<File> files;
	size_t bufferSize = 0;
	for (auto& p : paths) {
		try {
			auto& f = files.emplace_back(p + "/softwaredb.xml");
			bufferSize += f.getSize() + rapidsax::EXTRA_BUFFER_SPACE;
		} catch (MSXException& /*e*/) {
			// Ignore. It's not unusual the DB in the user
			// directory is not found. In case there's an error
			// with both user and system DB, we must give a
			// warning, but that's done below.
		}
	}
	buffer.resize(bufferSize);
	size_t bufferOffset = 0;
	for (auto& file : files) {
		try {
			auto size = file.getSize();
			auto* buf = &buffer[bufferOffset];
			bufferOffset += size + rapidsax::EXTRA_BUFFER_SPACE;
			file.read(buf, size);
			buf[size] = 0;

			parseDB(cliComm, buf, buffer.data(), db, unknownTypes);
		} catch (rapidsax::ParseError& e) {
			cliComm.printWarning(
				"Rom database parsing failed: ", e.what());
		} catch (MSXException& /*e*/) {
			// Ignore, see above
		}
	}
	if (bufferSize) buffer[0] = 0;
	if (db.empty()) {
		cliComm.printWarning(
			"Couldn't load software database.\n"
			"This may cause incorrect ROM mapper types to be used.");
	}
	if (!unknownTypes.empty()) {
		std::string output = "Unknown mapper types in software database: ";
		for (const auto& [type, count] : unknownTypes) {
			strAppend(output, type, " (", count, "x); ");
		}
		cliComm.printWarning(output);
	}
}

const RomInfo* RomDatabase::fetchRomInfo(const Sha1Sum& sha1sum) const
{
	auto it = ranges::lower_bound(db, sha1sum, {}, &Entry::sha1);
	return ((it != end(db)) && (it->sha1 == sha1sum))
		? &it->romInfo : nullptr;
}

} // namespace openmsx
