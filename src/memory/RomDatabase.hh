// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "noncopyable.hh"
#include <string>
#include <memory>
#include <map>
#include "StringOp.hh"

namespace openmsx {

class Rom;
class CliComm;
class RomInfo;
class XMLElement;
class SoftwareInfoTopic;
class GlobalCommandController;

class RomDatabase : private noncopyable
{
public:
	RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);
	~RomDatabase();

	/**
	 * Gives the info in the database for the given entry or NULL if the
	 * entry was not found.
	 */
	const RomInfo* fetchRomInfo(const std::string& sha1sum) const;

private:
	typedef std::map<std::string, RomInfo*, StringOp::caseless> DBMap;

	void initDatabase(CliComm& cliComm);
	std::string parseRemarks(const XMLElement& elem);
	void addEntry(CliComm& cliComm, std::auto_ptr<RomInfo> romInfo,
                     const std::string& sha1, DBMap& result);
	void parseEntry(CliComm& cliComm,
		const XMLElement& rom, DBMap& result,
		const std::string& title,   const std::string& year,
		const std::string& company, const std::string& country,
		bool original,              const std::string& origType,
		const std::string& remark,  const std::string& type);
	std::string parseStart(const XMLElement& rom);
	void parseDump(CliComm& cliComm,
		const XMLElement& dump, DBMap& result,
		const std::string& title,   const std::string& year,
		const std::string& company, const std::string& country,
		const std::string& remark);
	void parseSoftware(CliComm& cliComm, const std::string& filename,
                          const XMLElement& soft, DBMap& result);
	void parseDB(CliComm& cliComm, const std::string& filename,
                    const XMLElement& doc, DBMap& result);
	std::auto_ptr<XMLElement> openDB(CliComm& cliComm, const std::string& filename,
		const std::string& type);

	DBMap romDBSHA1;
	typedef std::map<std::string, unsigned> UnknownTypes;
	UnknownTypes unknownTypes;

	std::auto_ptr<SoftwareInfoTopic> softwareInfoTopic;
	friend class SoftwareInfoTopic;
};

} // namespace openmsx

#endif
