#include "InfoCommand.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "KeyRange.hh"
#include "unreachable.hh"
#include <iostream>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

InfoCommand::InfoCommand(CommandController& commandController_, const string& name_)
	: Command(commandController_, name_)
{
}

InfoCommand::~InfoCommand()
{
	assert(infoTopics.empty());
}

void InfoCommand::registerTopic(InfoTopic& topic)
{
#ifndef NDEBUG
	if (infoTopics.contains(topic.getName())) {
		std::cerr << "INTERNAL ERROR: already have an info topic with "
		             "name " << topic.getName() << std::endl;
		UNREACHABLE;
	}
#endif
	infoTopics.insert_noDuplicateCheck(&topic);
}

void InfoCommand::unregisterTopic(InfoTopic& topic)
{
	if (!infoTopics.contains(topic.getName())) {
		std::cerr << "INTERNAL ERROR: can't unregister topic with name "
		          << topic.getName() << ", not found!" << std::endl;
		UNREACHABLE;
	}
	infoTopics.erase(topic.getName());
}

// Command

void InfoCommand::execute(array_ref<TclObject> tokens,
                          TclObject& result)
{
	switch (tokens.size()) {
	case 1:
		// list topics
		for (auto* t : infoTopics) {
			result.addListElement(t->getName());
		}
		break;
	default:
		// show info about topic
		assert(tokens.size() >= 2);
		const auto& topic = tokens[1].getString();
		auto it = infoTopics.find(topic);
		if (it == end(infoTopics)) {
			throw CommandException("No info on: ", topic);
		}
		(*it)->execute(tokens, result);
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
		if (it == end(infoTopics)) {
			throw CommandException("No info on: ", tokens[1]);
		}
		result = (*it)->help(tokens);
		break;
	}
	return result;
}

void InfoCommand::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		// complete topic
		vector<string_view> topics;
		for (auto* t : infoTopics) {
			topics.emplace_back(t->getName());
		}
		completeString(tokens, topics);
		break;
	}
	default:
		// show help on a certain topic
		assert(tokens.size() >= 3);
		auto it = infoTopics.find(tokens[1]);
		if (it != end(infoTopics)) {
			(*it)->tabCompletion(tokens);
		}
		break;
	}
}

} // namespace openmsx
