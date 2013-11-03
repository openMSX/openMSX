#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "RomInfo.hh"
#include "MemBuffer.hh"
#include "sha1.hh"
#include "noncopyable.hh"
#include <utility>
#include <vector>
#include <memory>

namespace openmsx {

class CliComm;
class SoftwareInfoTopic;
class GlobalCommandController;

class RomDatabase : private noncopyable
{
public:
	typedef std::vector<std::pair<Sha1Sum, RomInfo>> RomDB;

	RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);
	~RomDatabase();

	/** Lookup an entry in the database by sha1sum.
	 * Returns nullptr when no corresponding entry was found.
	 */
	const RomInfo* fetchRomInfo(const Sha1Sum& sha1sum) const;

private:
	RomDB db;
	std::vector<MemBuffer<char>> buffers;
	std::unique_ptr<SoftwareInfoTopic> softwareInfoTopic;
};

} // namespace openmsx

#endif
