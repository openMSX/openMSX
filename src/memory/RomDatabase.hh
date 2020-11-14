#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "MemBuffer.hh"
#include "sha1.hh"
#include <utility>
#include <vector>

namespace openmsx {

class CliComm;
class RomInfo;

class RomDatabase
{
public:
	using RomDB = std::vector<std::pair<Sha1Sum, RomInfo>>;

	RomDatabase(CliComm& cliComm);

	/** Lookup an entry in the database by sha1sum.
	 * Returns nullptr when no corresponding entry was found.
	 */
	[[nodiscard]] const RomInfo* fetchRomInfo(const Sha1Sum& sha1sum) const;

	[[nodiscard]] const char* getBufferStart() const { return buffer.data(); }

private:
	RomDB db;
	MemBuffer<char> buffer;
};

} // namespace openmsx

#endif
