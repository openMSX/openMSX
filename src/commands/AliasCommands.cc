// $Id$

#include <cassert>
#include "CommandController.hh"
#include "AliasCommands.hh"

namespace openmsx {

AliasCommands::AliasCommands(CommandController& controller_)
	: controller(controller_), aliasCmd(*this), unaliasCmd(*this)
{
	controller.registerCommand(&aliasCmd, "alias");
	controller.registerCommand(&unaliasCmd, "unalias");
}

AliasCommands::~AliasCommands()
{
	for (map<string, Alias*>::const_iterator it = aliasses.begin();
	     it != aliasses.end(); ++it) {
		delete it->second;
	}
	
	controller.unregisterCommand(&aliasCmd, "alias");
	controller.unregisterCommand(&unaliasCmd, "unalias");
}

void AliasCommands::getAliases(set<string>& result) const
{
	for (map<string, Alias*>::const_iterator it = aliasses.begin();
	     it != aliasses.end(); ++it) {
		result.insert(it->first);
	}
}


// alias framework
AliasCommands::Alias::Alias(AliasCommands& parent_,
	                    const string& name_, const string& definition_)
	: parent(parent_), name(name_), definition(definition_)
{
	parent.controller.registerCommand(this, name);
}

AliasCommands::Alias::~Alias()
{
	parent.controller.unregisterCommand(this, name);
}

const string& AliasCommands::Alias::getName() const
{
	return name;
}

const string& AliasCommands::Alias::getDefinition() const
{
	return definition;
}

string AliasCommands::Alias::execute(const vector<string> &tokens)
	throw(CommandException)
{
	return parent.controller.executeCommand(definition);
}

string AliasCommands::Alias::help(const vector<string> &tokens) const
	throw()
{
	return name + " is aliased to '" + definition + "'";
}


// Alias command
AliasCommands::AliasCmd::AliasCmd(AliasCommands& parent_)
	: parent(parent_)
{
}

string AliasCommands::AliasCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	switch (tokens.size()) {
	case 1:
		for (map<string, Alias*>::const_iterator it =
		         parent.aliasses.begin();
		     it != parent.aliasses.end(); ++it) {
			result += it->first + ": " +
			          it->second->getDefinition() + '\n';
		}
		break;
	case 2: {
		map<string, Alias*>::const_iterator it =
			parent.aliasses.find(tokens[1]);
		if (it == parent.aliasses.end()) {
			throw CommandException(tokens[1] + ": no such alias");
		}
		result = it->second->getDefinition();
		break;
		}
	default: {
		assert(tokens.size() >= 3);
		string definition = tokens[2];
		for (unsigned i = 3; i < tokens.size(); ++i) {
			definition += ' ' + tokens[i];
		}
		map<string, Alias*>::const_iterator it =
			parent.aliasses.find(tokens[1]);
		if (it != parent.aliasses.end()) {
			delete it->second;
		}
		if (CommandController::instance()->hasCommand(tokens[1])) {
			throw CommandException("Cannot redefine command as alias");
		}
		parent.aliasses[tokens[1]] =
			new Alias(parent, tokens[1], definition);
	}
	}
	return result;
}

string AliasCommands::AliasCmd::help(const vector<string> &tokens) const
	throw()
{
	return "alias                   show all aliasses\n"
	       "alias <name>            show the definition of this alias\n"
	       "alias <name> <command>  (re)define an alias\n";
}

void AliasCommands::AliasCmd::tabCompletion(vector<string> &tokens) const
	throw()
{
	if (tokens.size() == 2) {
		set<string> aliasses;
		parent.getAliases(aliasses);
		CommandController::completeString(tokens, aliasses);
	}
}

// UnaliasCmd command
AliasCommands::UnaliasCmd::UnaliasCmd(AliasCommands& parent_)
	:parent(parent_)
{
}

string AliasCommands::UnaliasCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	if (tokens.size() != 2) {
		throw CommandException("Syntax error");
	}
	map<string, Alias*>::const_iterator it =
		parent.aliasses.find(tokens[1]);
	if (it == parent.aliasses.end()) {
		throw CommandException(tokens[1] + ": was not alised");
	}
	delete it->second;
	parent.aliasses.erase(tokens[1]);
	return "";
}

string AliasCommands::UnaliasCmd::help(const vector<string> &tokens) const
	throw()
{
	return "unalias <name>  remove the given alias";
}

void AliasCommands::UnaliasCmd::tabCompletion(vector<string> &tokens) const
	throw()
{
	if (tokens.size() == 2) {
		set<string> aliasses;
		parent.getAliases(aliasses);
		CommandController::completeString(tokens, aliasses);
	}
}

} // namespace openmsx
