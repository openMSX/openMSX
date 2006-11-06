// $Id$

#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Rom;
class GlobalCliComm;
class RomInfo;

class RomDatabase : private noncopyable
{
public:
	static RomDatabase& instance();

	std::auto_ptr<RomInfo> fetchRomInfo(
		GlobalCliComm& cliComm, const Rom& rom);

private:
	RomDatabase();
	~RomDatabase();
};

} // namespace openmsx

#endif
