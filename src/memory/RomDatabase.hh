// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include <memory>

namespace openmsx {

class Rom;
class CliComm;
class RomInfo;

class RomDatabase
{
public:
	static RomDatabase& instance();

	std::auto_ptr<RomInfo> fetchRomInfo(CliComm& cliComm, const Rom& rom);

private:
	RomDatabase();
	~RomDatabase();
};

} // namespace openmsx

#endif
