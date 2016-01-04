#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "RomInfo.hh"
#include "MemBuffer.hh"
#include "InfoTopic.hh"
#include "sha1.hh"
#include <utility>
#include <vector>

namespace openmsx {

class CliComm;
class GlobalCommandController;

class RomDatabase
{
public:
	using RomDB = std::vector<std::pair<Sha1Sum, RomInfo>>;

	RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);

	/** Lookup an entry in the database by sha1sum.
	 * Returns nullptr when no corresponding entry was found.
	 */
	const RomInfo* fetchRomInfo(const Sha1Sum& sha1sum) const;

	const char* getBufferStart() const { return buffer.data(); }

private:
	RomDB db;
	MemBuffer<char> buffer;

	struct SoftwareInfoTopic final : InfoTopic {
		SoftwareInfoTopic(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} softwareInfoTopic;
};

} // namespace openmsx

#endif
