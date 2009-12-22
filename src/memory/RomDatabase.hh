// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class Rom;
class CliComm;
class RomInfo;
class SoftwareInfoTopic;
class GlobalCommandController;

class RomDatabase : private noncopyable
{
public:
	explicit RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);
	~RomDatabase();

	/**
	 * Gives the info in the database for the given entry or NULL if the
	 * entry was not found.
	 */
	const RomInfo* fetchRomInfo(const std::string& sha1sum) const;

private:
	void initDatabase(CliComm& cliComm);

	std::auto_ptr<SoftwareInfoTopic> softwareInfoTopic;
	friend class SoftwareInfoTopic;
};

} // namespace openmsx

#endif
