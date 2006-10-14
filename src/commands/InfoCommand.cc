// $Id$

#include "InfoCommand.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "MSXCommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include <iostream>
#include <cassert>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

InfoCommand::InfoCommand(CommandController& commandController, const string& name,
                         Reactor* reactor_)
	: Command(commandController, name)
	, reactor(reactor_)
{
}

InfoCommand::~InfoCommand()
{
	assert(infoTopics.empty());
}

void InfoCommand::registerTopic(InfoTopic& topic, const string& name)
{
#ifndef NDEBUG
	if (infoTopics.find(name) != infoTopics.end()) {
		std::cerr << "INTERNAL ERROR: already have a info topic with "
		             "name " << name << std::endl;
		assert(false);
	}
#endif
	infoTopics[name] = &topic;
}

void InfoCommand::unregisterTopic(InfoTopic& topic, const string& name)
{
	(void)topic;
	if (infoTopics.find(name) == infoTopics.end()) {
		std::cerr << "INTERNAL ERROR: can't unregister topic with name "
			"name " << name << ", not found!" << std::endl;
		assert(false);
	}
	assert(infoTopics[name] == &topic);
	infoTopics.erase(name);
}

bool InfoCommand::hasTopic(const std::string& name) const
{
	return infoTopics.find(name) != infoTopics.end();
}

// Command

bool InfoCommand::executeBWC(const vector<TclObject*>& tokens,
                             TclObject& result)
{
	if (!reactor) return false;
	MSXMotherBoard* motherBoard = reactor->getMotherBoard();
	if (!motherBoard) return false;
	MSXCommandController& msxCommandController =
		motherBoard->getMSXCommandController();
	InfoCommand& machineInfoCommand =
		msxCommandController.getMachineInfoCommand();
	string topic = tokens[1]->getString();
	if (!machineInfoCommand.hasTopic(topic)) return false;
	msxCommandController.getCliComm().printWarning(
		"'openmsx_info " + topic + "' is deprecated, "
		"please use 'machine_info " + topic + "'.");
	machineInfoCommand.execute(tokens, result);
	return true;
}

void InfoCommand::execute(const vector<TclObject*>& tokens,
                          TclObject& result)
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
		string topic = tokens[1]->getString();
		map<string, const InfoTopic*>::const_iterator it =
			infoTopics.find(topic);
		if (it == infoTopics.end()) {
			// backwards compatibility: also try machine_info
			if (executeBWC(tokens, result)) return;
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
		completeString(tokens, topics);
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

} // namespace openmsx
