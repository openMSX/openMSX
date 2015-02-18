#ifndef ROMDATABASE_HH
#define ROMDATABASE_HH

#include "RomInfo.hh"
#include "MemBuffer.hh"
#include "InfoTopic.hh"
#include "sha1.hh"
#include "noncopyable.hh"
#include <utility>
#include <vector>

namespace openmsx {

class CliComm;
class GlobalCommandController;

class RomDatabase : private noncopyable
{
public:
	using RomDB = std::vector<std::pair<Sha1Sum, RomInfo>>;

	RomDatabase(GlobalCommandController& commandController, CliComm& cliComm);

	/** Lookup an entry in the database by sha1sum.
	 * Returns nullptr when no corresponding entry was found.
	 */
	const RomInfo* fetchRomInfo(const Sha1Sum& sha1sum) const;

private:
	RomDB db;
	std::vector<MemBuffer<char>> buffers;

	class SoftwareInfoTopic final : public InfoTopic {
	public:
		SoftwareInfoTopic(InfoCommand& openMSXInfoCommand,
		                  RomDatabase& romDatabase);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		const RomDatabase& romDatabase;
	} softwareInfoTopic;
};

} // namespace openmsx

#endif
