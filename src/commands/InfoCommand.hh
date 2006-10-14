// $Id$

#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include <map>

namespace openmsx {

class InfoTopic;
class Reactor;

class InfoCommand : public Command
{
public:
	InfoCommand(CommandController& commandController, const std::string& name,
	            Reactor* reactor = NULL);
	virtual ~InfoCommand();

	void   registerTopic(InfoTopic& topic, const std::string& name);
	void unregisterTopic(InfoTopic& topic, const std::string& name);

	bool hasTopic(const std::string& name) const;

	// Command
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	bool executeBWC(const std::vector<TclObject*>& tokens,
	                TclObject& result);

	std::map<std::string, const InfoTopic*> infoTopics;
	Reactor* reactor; // for bwc
};

} // namespace openmsx

#endif
