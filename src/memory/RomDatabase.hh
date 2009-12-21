// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "noncopyable.hh"
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
	explicit RomDatabase(GlobalCommandController& commandController);
	~RomDatabase();

	std::auto_ptr<RomInfo> fetchRomInfo(CliComm& cliComm, const Rom& rom);

private:
	std::auto_ptr<SoftwareInfoTopic> softwareInfoTopic;
	friend class SoftwareInfoTopic;
};

} // namespace openmsx

#endif
