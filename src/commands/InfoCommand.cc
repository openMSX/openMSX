// $Id$

#include <cassert>
#include "config.h"
#include "InfoCommand.hh"
#include "CommandController.hh"
#include "CommandResult.hh"

namespace openmsx {

InfoCommand::InfoCommand()
{
	registerTopic("version", &versionInfo);
}

InfoCommand::~InfoCommand()
{
	unregisterTopic("version", &versionInfo);
}

InfoCommand& InfoCommand::instance()
{
	static InfoCommand oneInstance;
	return oneInstance;
}

void InfoCommand::registerTopic(const string& name,
                                const InfoTopic* topic)
{
	assert(infoTopics.find(name) == infoTopics.end());
	infoTopics[name] = topic;
}

void InfoCommand::unregisterTopic(const string& name,
                                  const InfoTopic* topic)
{
	assert(infoTopics.find(name) != infoTopics.end());
	assert(infoTopics[name] == topic);
	infoTopics.erase(name);
}

// Command

void InfoCommand::execute(const vector<string>& tokens, CommandResult& result)
	throw (CommandException)
{
	switch (tokens.size()) {
	case 1:
		// list topics
		for (map<string, const InfoTopic*>::const_iterator it =
		       infoTopics.begin(); it != infoTopics.end(); ++it) {
			result.addListElement(it->first);
		}
		break;
	default:
		// show info about topic
		assert(tokens.size() >= 2);
		map<string, const InfoTopic*>::const_iterator it =
			infoTopics.find(tokens[1]);
		if (it == infoTopics.end()) {
			throw CommandException("No info on: " + tokens[1]);
		}
		it->second->execute(tokens, result);
		break;
	}
}

string InfoCommand::help(const vector<string> &tokens) const
	throw(CommandException)
{
	string result;
	switch (tokens.size()) {
	case 1:
		// show help on info cmd
		result = "Show info on a certain topic\n"
		         " info [topic] [...]\n";
		break;
	default:
		// show help on a certain topic
		assert(tokens.size() >= 2);
		map<string, const InfoTopic*>::const_iterator it =
			infoTopics.find(tokens[1]);
		if (it == infoTopics.end()) {
			throw CommandException("No info on: " + tokens[1]);
		}
		result = it->second->help(tokens);
		break;
	}
	return result;
}

void InfoCommand::tabCompletion(vector<string> &tokens) const throw()
{
	switch (tokens.size()) {
	case 2: {
		// complete topic
		set<string> topics;
		for (map<string, const InfoTopic*>::const_iterator it =
		       infoTopics.begin(); it != infoTopics.end(); ++it) {
			topics.insert(it->first);
		}
		CommandController::completeString(tokens, topics);
		break;
	}
	default:
		// show help on a certain topic
		assert(tokens.size() >= 3);
		map<string, const InfoTopic*>::const_iterator it =
			infoTopics.find(tokens[1]);
		if (it != infoTopics.end()) {
			it->second->tabCompletion(tokens);
		}
		break;
	}
}

// Version info

void InfoCommand::VersionInfo::execute(const vector<string> &tokens,
                                       CommandResult& result) const
	throw()
{
	result.setString("openMSX " PACKAGE_VERSION " -- built on "__DATE__);
}

string InfoCommand::VersionInfo::help(const vector<string> &tokens) const
	throw()
{
	return "Prints openMSX version.";
}

} // namespace openmsx
