#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "hash_set.hh"
#include "xxhash.hh"

namespace openmsx {

class InfoCommand final : public Command
{
public:
	InfoCommand(CommandController& commandController, const std::string& name);
	~InfoCommand();

	void   registerTopic(const InfoTopic& topic);
	void unregisterTopic(const InfoTopic& topic);

private:
	// Command
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	struct NameFromInfoTopic {
		[[nodiscard]] const std::string& operator()(const InfoTopic* t) const {
			return t->getName();
		}
	};
	hash_set<const InfoTopic*, NameFromInfoTopic, XXHasher> infoTopics;
};

} // namespace openmsx

#endif
