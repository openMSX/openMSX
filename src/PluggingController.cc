// $Id$

#include <cassert>
#include <cstdio>
#include "CommandController.hh"
#include "Scheduler.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "PluggableFactory.hh"
#include "openmsx.hh"
#include "InfoCommand.hh"
#include "CommandArgument.hh"
#include "CliCommOutput.hh"

namespace openmsx {

PluggingController::PluggingController()
	: plugCmd(*this),
	  unplugCmd(*this),
	  pluggableInfo(*this),
	  connectorInfo(*this),
	  connectionClassInfo(*this),
	  scheduler(Scheduler::instance()),
	  commandController(CommandController::instance()),
	  infoCommand(InfoCommand::instance())
{
	PluggableFactory::createAll(this);

	commandController.registerCommand(&plugCmd,   "plug");
	commandController.registerCommand(&unplugCmd, "unplug");
	infoCommand.registerTopic("pluggable", &pluggableInfo);
	infoCommand.registerTopic("connector", &connectorInfo);
	infoCommand.registerTopic("connectionclass", &connectionClassInfo);
}

PluggingController::~PluggingController()
{
	commandController.unregisterCommand(&plugCmd,   "plug");
	commandController.unregisterCommand(&unplugCmd, "unplug");
	infoCommand.unregisterTopic("pluggable", &pluggableInfo);
	infoCommand.unregisterTopic("connector", &connectorInfo);
	infoCommand.unregisterTopic("connectionclass", &connectionClassInfo);

#ifndef NDEBUG
	// This is similar to an assert: it should never print anything,
	// but if it does, it helps to catch an error.
	for (vector<Connector*>::const_iterator it = connectors.begin();
		it != connectors.end(); ++it
	) {
		fprintf(stderr,
			"ERROR: Connector still plugged at shutdown: %s\n",
			(*it)->getName().c_str()
			);
	}
#endif
	for (vector<Pluggable*>::iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		delete *it;
	}
}

PluggingController& PluggingController::instance()
{
	static PluggingController oneInstance;
	return oneInstance;
}

void PluggingController::registerConnector(Connector* connector)
{
	connectors.push_back(connector);
}

void PluggingController::unregisterConnector(Connector* connector)
{
	for (vector<Connector*>::iterator it = connectors.begin();
	     it != connectors.end(); ++it) {
		if ((*it) == connector) {
			connectors.erase(it);
			return;
		}
	}
}


void PluggingController::registerPluggable(Pluggable* pluggable)
{
	pluggables.push_back(pluggable);
}

void PluggingController::unregisterPluggable(Pluggable* pluggable)
{
	for (vector<Pluggable*>::iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		if ((*it) == pluggable) {
			pluggables.erase(it);
			return;
		}
	}
}

// === Commands ===
//  plug command

PluggingController::PlugCmd::PlugCmd(PluggingController& parent_)
	: parent(parent_)
{
}

string PluggingController::PlugCmd::execute(const vector<string>& tokens)
{
	string result;
	const EmuTime &time = parent.scheduler.getCurrentTime();
	switch (tokens.size()) {
		case 1: {
			for (vector<Connector *>::const_iterator it =
			                       parent.connectors.begin();
			     it != parent.connectors.end();
			     ++it) {
				result += ((*it)->getName() + ": " +
				           (*it)->getPlugged().getName()) + '\n';
			}
			break;
		}
		case 2: {
			Connector *connector = parent.getConnector(tokens[1]);
			if (connector == NULL) {
				throw CommandException("plug: " + tokens[1] + ": no such connector");
			}
			result += (connector->getName() + ": " +
			           connector->getPlugged().getName()) + '\n';
			break;
		}
		case 3: {
			Connector *connector = parent.getConnector(tokens[1]);
			if (connector == NULL) {
				throw CommandException("plug: " + tokens[1] + ": no such connector");
			}
			Pluggable *pluggable = parent.getPluggable(tokens[2]);
			if (pluggable == NULL) {
				throw CommandException("plug: " + tokens[2] + ": no such pluggable");
			}
			if (connector->getClass() != pluggable->getClass()) {
				throw CommandException("plug: " + tokens[2] + " doesn't fit in " + tokens[1]);
			}
			connector->unplug(time);
			try {
				connector->plug(pluggable, time);
				CliCommOutput::instance().update(
					CliCommOutput::PLUG, tokens[1], tokens[2]);
			} catch (PlugException &e) {
				throw CommandException("plug: plug failed: " + e.getMessage());
			}
			break;
		}
	default:
		throw SyntaxError();
	}
	return result;
}

string PluggingController::PlugCmd::help(const vector<string>& /*tokens*/) const
{
	return "Plugs a plug into a connector\n"
	       " plug [connector] [plug]\n";
}

void PluggingController::PlugCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (vector<Connector*>::const_iterator it =
			               parent.connectors.begin();
		     it != parent.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		CommandController::completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		set<string> pluggables;
		Connector* connector = parent.getConnector(tokens[1]);
		string className = connector ? connector->getClass() : "";
		for (vector<Pluggable*>::const_iterator it =
			 parent.pluggables.begin();
		     it != parent.pluggables.end(); ++it) {
			Pluggable* pluggable = *it;
			if (pluggable->getClass() == className) {
				pluggables.insert(pluggable->getName());
			}
		}
		CommandController::completeString(tokens, pluggables);
	}
}


//  unplug command

PluggingController::UnplugCmd::UnplugCmd(PluggingController& parent_)
	: parent(parent_)
{
}

string PluggingController::UnplugCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	Connector *connector = parent.getConnector(tokens[1]);
	if (connector == NULL) {
		throw CommandException("No such connector");
	}
	const EmuTime &time = parent.scheduler.getCurrentTime();
	connector->unplug(time);
	CliCommOutput::instance().update(CliCommOutput::UNPLUG, tokens[1], "");
	return "";
}

string PluggingController::UnplugCmd::help(const vector<string>& /*tokens*/) const
{
	return "Unplugs a plug from a connector\n"
	       " unplug [connector]\n";
}

void PluggingController::UnplugCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (vector<Connector *>::const_iterator it =
		                       parent.connectors.begin();
		     it != parent.connectors.end();
		     ++it) {
			connectors.insert((*it)->getName());
		}
		CommandController::completeString(tokens, connectors);
	}
}

Connector *PluggingController::getConnector(const string& name)
{
	for (vector<Connector *>::const_iterator it = connectors.begin();
	     it != connectors.end();
	     ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

Pluggable *PluggingController::getPluggable(const string& name)
{
	for (vector<Pluggable *>::const_iterator it = pluggables.begin();
	     it != pluggables.end();
	     ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}


// Pluggable info

PluggingController::PluggableInfo::PluggableInfo(PluggingController& parent_)
	: parent(parent_)
{
}

void PluggingController::PluggableInfo::execute(const vector<CommandArgument>& tokens,
	CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2:
		for (vector<Pluggable*>::const_iterator it =
			 parent.pluggables.begin();
		     it != parent.pluggables.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Pluggable* pluggable = parent.getPluggable(tokens[2].getString());
		if (!pluggable) {
			throw CommandException("No such pluggable");
		}
		result.setString(pluggable->getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::PluggableInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available pluggables. "
	       "Or show info on a specific pluggable.\n";
}

void PluggingController::PluggableInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> pluggables;
		for (vector<Pluggable*>::const_iterator it =
			 parent.pluggables.begin();
		     it != parent.pluggables.end(); ++it) {
			pluggables.insert((*it)->getName());
		}
		CommandController::completeString(tokens, pluggables);
	}
}

// Connector info

PluggingController::ConnectorInfo::ConnectorInfo(PluggingController& parent_)
	: parent(parent_)
{
}

void PluggingController::ConnectorInfo::execute(const vector<CommandArgument>& tokens,
	CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2:
		for (vector<Connector*>::const_iterator it =
			 parent.connectors.begin();
		     it != parent.connectors.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Connector* connector = parent.getConnector(tokens[2].getString());
		if (!connector) {
			throw CommandException("No such connector");
		}
		result.setString(connector->getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::ConnectorInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available connectors.\n";
}

void PluggingController::ConnectorInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> connectors;
		for (vector<Connector*>::const_iterator it =
			               parent.connectors.begin();
		     it != parent.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		CommandController::completeString(tokens, connectors);
	}
}

// Connection Class info

PluggingController::ConnectionClassInfo::ConnectionClassInfo(PluggingController& parent_)
	: parent(parent_)
{
}

void PluggingController::ConnectionClassInfo::execute(const vector<CommandArgument>& tokens,
	CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> classes;
		for (vector<Connector*>::const_iterator it =
			 parent.connectors.begin();
		     it != parent.connectors.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		for (vector<Pluggable*>::const_iterator it =
			 parent.pluggables.begin();
		     it != parent.pluggables.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		for (set<string>::const_iterator it = classes.begin();
		     it != classes.end(); ++it) {
			result.addListElement(*it);
		}
		break;
	}
	case 3: {
		string arg = tokens[2].getString();
		const Connector* connector = parent.getConnector(arg);
		if (connector) {
			result.setString(connector->getClass());
			break;
		}
		const Pluggable* pluggable = parent.getPluggable(arg);
		if (pluggable) {
			result.setString(pluggable->getClass());
			break;
		}
		throw CommandException("No such connector or pluggable");
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::ConnectionClassInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows the class a connector or pluggable belongs to.";
}

void PluggingController::ConnectionClassInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> names;
		for (vector<Connector*>::const_iterator it =
			               parent.connectors.begin();
		     it != parent.connectors.end(); ++it) {
			names.insert((*it)->getName());
		}
		for (vector<Pluggable*>::const_iterator it =
			               parent.pluggables.begin();
		     it != parent.pluggables.end(); ++it) {
			names.insert((*it)->getName());
		}
		CommandController::completeString(tokens, names);
	}
}

} // namespace openmsx
