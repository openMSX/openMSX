#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include "StringMap.hh"

namespace openmsx {

class InfoTopic;

class InfoCommand : public Command
{
public:
	InfoCommand(CommandController& commandController, const std::string& name);
	virtual ~InfoCommand();

	void   registerTopic(InfoTopic& topic, string_ref name);
	void unregisterTopic(InfoTopic& topic, string_ref name);

private:
	// Command
	virtual void execute(array_ref<TclObject> tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	StringMap<const InfoTopic*> infoTopics;
};

} // namespace openmsx

#endif
