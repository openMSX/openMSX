#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "hash_set.hh"
#include "xxhash.hh"

namespace openmsx {

class InfoTopic;

class InfoCommand final : public Command
{
public:
	InfoCommand(CommandController& commandController, const std::string& name);
	~InfoCommand();

	void   registerTopic(InfoTopic& topic);
	void unregisterTopic(InfoTopic& topic);

private:
	// Command
	void execute(array_ref<TclObject> tokens,
	             TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	struct NameFromInfoTopic {
		const std::string& operator()(const InfoTopic* t) const {
			return t->getName();
		}
	};
	hash_set<const InfoTopic*, NameFromInfoTopic, XXHasher> infoTopics;
};

} // namespace openmsx

#endif
