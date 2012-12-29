// $Id$

#include "InfoCommand.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "MSXCommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "KeyRange.hh"
#include "unreachable.hh"
#include <iostream>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

InfoCommand::InfoCommand(CommandController& commandController, const string& name)
	: Command(commandController, name)
{
}

InfoCommand::~InfoCommand()
{
	assert(infoTopics.empty());
}

void InfoCommand::registerTopic(InfoTopic& topic, string_ref name)
{
#ifndef NDEBUG
	if (infoTopics.find(name) != infoTopics.end()) {
		std::cerr << "INTERNAL ERROR: already have a info topic with "
		             "name " << name << std::endl;
		UNREACHABLE;
	}
#endif
	infoTopics[name] = &topic;
}

void InfoCommand::unregisterTopic(InfoTopic& topic, string_ref name)
{
	(void)topic;
	if (infoTopics.find(name) == infoTopics.end()) {
		std::cerr << "INTERNAL ERROR: can't unregister topic with name "
			"name " << name << ", not found!" << std::endl;
		UNREACHABLE;
	}
	assert(infoTopics[name] == &topic);
	infoTopics.erase(name);
}

// Command

void InfoCommand::execute(const vector<TclObject>& tokens,
                          TclObject& result)
{
	switch (tokens.size()) {
	case 1:
		// list topics
		for (auto& p : infoTopics) {
			result.addListElement(p.first());
		}
		break;
	default:
		// show info about topic
		assert(tokens.size() >= 2);
		string_ref topic = tokens[1].getString();
		auto it = infoTopics.find(topic);
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
		auto it = infoTopics.find(tokens[1]);
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
		completeString(tokens, keys(infoTopics));
		break;
	}
	default:
		// show help on a certain topic
		assert(tokens.size() >= 3);
		auto it = infoTopics.find(tokens[1]);
		if (it != infoTopics.end()) {
			it->second->tabCompletion(tokens);
		}
		break;
	}
}

} // namespace openmsx
