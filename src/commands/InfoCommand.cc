// $Id$

#include "InfoCommand.hh"
#include "CommandController.hh"
#include "CommandArgument.hh"
#include "CommandException.hh"
#include "RomInfoTopic.hh"
#include "Version.hh"
#include <cassert>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

InfoCommand::InfoCommand()
	: romInfoTopic(new RomInfoTopic())
{
	registerTopic("version", &versionInfo);
	registerTopic("romtype", romInfoTopic.get());
}

InfoCommand::~InfoCommand()
{
	unregisterTopic("romtype", romInfoTopic.get());
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

void InfoCommand::execute(const vector<CommandArgument>& tokens,
                          CommandArgument& result)
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
		string topic = tokens[1].getString();
		map<string, const InfoTopic*>::const_iterator it =
			infoTopics.find(topic);
		if (it == infoTopics.end()) {
			throw CommandException("No info on: " + topic);
		}
		it->second->execute(tokens, result);
		break;
	}
}

string InfoCommand::help(const vector<string>& tokens) const
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

void InfoCommand::tabCompletion(vector<string>& tokens) const
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

void InfoCommand::VersionInfo::execute(const vector<CommandArgument>& /*tokens*/,
                                       CommandArgument& result) const
{
	result.setString(Version::FULL_VERSION);
}

string InfoCommand::VersionInfo::help(const vector<string>& /*tokens*/) const
{
	return "Prints openMSX version.";
}

} // namespace openmsx
