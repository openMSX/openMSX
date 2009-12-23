// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "StringOp.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>
#include <map>

namespace openmsx {

class CliComm;
class RomInfo;
class SoftwareInfoTopic;
class GlobalCommandController;

class RomDatabase : private noncopyable
{
public:
	typedef std::map<std::string, RomInfo*, StringOp::caseless> DBMap;

	RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);
	~RomDatabase();

	/** Lookup an entry in the database by sha1sum.
	 * Returns NULL when no corresponding entry was found.
	 */
	const RomInfo* fetchRomInfo(const std::string& sha1sum) const;

private:
	DBMap romDBSHA1;
	std::auto_ptr<SoftwareInfoTopic> softwareInfoTopic;
};

} // namespace openmsx

#endif
