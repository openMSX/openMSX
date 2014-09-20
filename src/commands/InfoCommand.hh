#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include "StringMap.hh"

namespace openmsx {

class InfoTopic;

class InfoCommand final : public Command
{
public:
	InfoCommand(CommandController& commandController, const std::string& name);
	~InfoCommand();

	void   registerTopic(InfoTopic& topic, string_ref name);
	void unregisterTopic(InfoTopic& topic, string_ref name);

private:
	// Command
	void execute(array_ref<TclObject> tokens,
	             TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	StringMap<const InfoTopic*> infoTopics;
};

} // namespace openmsx

#endif
