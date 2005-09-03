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
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"

using std::set;
using std::string;
using std::vector;

namespace openmsx {

PluggingController::PluggingController(MSXMotherBoard& motherBoard)
	: plugCmd            (motherBoard.getCommandController(), *this)
	, unplugCmd          (motherBoard.getCommandController(), *this)
	, pluggableInfo      (motherBoard.getCommandController(), *this)
	, connectorInfo      (motherBoard.getCommandController(), *this)
	, connectionClassInfo(motherBoard.getCommandController(), *this)
	, cliComm(motherBoard.getCliComm())
	, scheduler(motherBoard.getScheduler())
{
	PluggableFactory::createAll(*this, motherBoard);
}

PluggingController::~PluggingController()
{
#ifndef NDEBUG
	// This is similar to an assert: it should never print anything,
	// but if it does, it helps to catch an error.
	for (Connectors::const_iterator it = connectors.begin();
		it != connectors.end(); ++it
	) {
		fprintf(stderr,
			"ERROR: Connector still plugged at shutdown: %s\n",
			(*it)->getName().c_str()
			);
	}
#endif
	for (Pluggables::iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		delete *it;
	}
}

void PluggingController::registerConnector(Connector* connector)
{
	connectors.push_back(connector);
}

void PluggingController::unregisterConnector(Connector* connector)
{
	for (Connectors::iterator it = connectors.begin();
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
	for (Pluggables::iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		if ((*it) == pluggable) {
			pluggables.erase(it);
			return;
		}
	}
}

// === Commands ===
//  plug command

PluggingController::PlugCmd::PlugCmd(CommandController& commandController,
                                     PluggingController& pluggingController_)
	: SimpleCommand(commandController, "plug")
	, pluggingController(pluggingController_)
{
}

string PluggingController::PlugCmd::execute(const vector<string>& tokens)
{
	string result;
	const EmuTime &time = pluggingController.scheduler.getCurrentTime();
	switch (tokens.size()) {
	case 1: {
		for (Connectors::const_iterator it =
				       pluggingController.connectors.begin();
		     it != pluggingController.connectors.end();
		     ++it) {
			result += ((*it)->getName() + ": " +
				   (*it)->getPlugged().getName()) + '\n';
		}
		break;
	}
	case 2: {
		Connector *connector = pluggingController.getConnector(tokens[1]);
		if (connector == NULL) {
			throw CommandException("plug: " + tokens[1] + ": no such connector");
		}
		result += (connector->getName() + ": " +
			   connector->getPlugged().getName()) + '\n';
		break;
	}
	case 3: {
		Connector *connector = pluggingController.getConnector(tokens[1]);
		if (connector == NULL) {
			throw CommandException("plug: " + tokens[1] + ": no such connector");
		}
		Pluggable *pluggable = pluggingController.getPluggable(tokens[2]);
		if (pluggable == NULL) {
			throw CommandException("plug: " + tokens[2] + ": no such pluggable");
		}
		if (connector->getClass() != pluggable->getClass()) {
			throw CommandException("plug: " + tokens[2] +
			                       " doesn't fit in " + tokens[1]);
		}
		connector->unplug(time);
		try {
			connector->plug(pluggable, time);
			pluggingController.cliComm.update(
				CliComm::PLUG, tokens[1], tokens[2]);
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
		for (Connectors::const_iterator it =
			               pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		set<string> pluggables;
		Connector* connector = pluggingController.getConnector(tokens[1]);
		string className = connector ? connector->getClass() : "";
		for (Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			Pluggable* pluggable = *it;
			if (pluggable->getClass() == className) {
				pluggables.insert(pluggable->getName());
			}
		}
		completeString(tokens, pluggables);
	}
}


//  unplug command

PluggingController::UnplugCmd::UnplugCmd(CommandController& commandController,
                                         PluggingController& pluggingController_)
	: SimpleCommand(commandController, "unplug")
	, pluggingController(pluggingController_)
{
}

string PluggingController::UnplugCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	Connector *connector = pluggingController.getConnector(tokens[1]);
	if (connector == NULL) {
		throw CommandException("No such connector");
	}
	const EmuTime &time = pluggingController.scheduler.getCurrentTime();
	connector->unplug(time);
	pluggingController.cliComm.update(CliComm::UNPLUG, tokens[1], "");
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
		for (Connectors::const_iterator it =
		                       pluggingController.connectors.begin();
		     it != pluggingController.connectors.end();
		     ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	}
}

Connector *PluggingController::getConnector(const string& name)
{
	for (Connectors::const_iterator it = connectors.begin();
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
	for (Pluggables::const_iterator it = pluggables.begin();
	     it != pluggables.end();
	     ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}


// Pluggable info

PluggingController::PluggableInfo::PluggableInfo(
		CommandController& commandController,
		PluggingController& pluggingController_)
	: InfoTopic(commandController, "pluggable")
	, pluggingController(pluggingController_)
{
}

void PluggingController::PluggableInfo::execute(const vector<TclObject*>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Pluggable* pluggable = pluggingController.getPluggable(
				tokens[2]->getString());
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
		for (Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			pluggables.insert((*it)->getName());
		}
		completeString(tokens, pluggables);
	}
}

// Connector info

PluggingController::ConnectorInfo::ConnectorInfo(
		CommandController& commandController,
		PluggingController& pluggingController_)
	: InfoTopic(commandController, "connector")
	, pluggingController(pluggingController_)
{
}

void PluggingController::ConnectorInfo::execute(const vector<TclObject*>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (Connectors::const_iterator it =
			 pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Connector* connector = pluggingController.getConnector(tokens[2]->getString());
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
		for (Connectors::const_iterator it =
			               pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	}
}

// Connection Class info

PluggingController::ConnectionClassInfo::ConnectionClassInfo(
		CommandController& commandController,
		PluggingController& pluggingController_)
	: InfoTopic(commandController, "connectionclass")
	, pluggingController(pluggingController_)
{
}

void PluggingController::ConnectionClassInfo::execute(const vector<TclObject*>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> classes;
		for (Connectors::const_iterator it =
			 pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		for (Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		for (set<string>::const_iterator it = classes.begin();
		     it != classes.end(); ++it) {
			result.addListElement(*it);
		}
		break;
	}
	case 3: {
		string arg = tokens[2]->getString();
		const Connector* connector = pluggingController.getConnector(arg);
		if (connector) {
			result.setString(connector->getClass());
			break;
		}
		const Pluggable* pluggable = pluggingController.getPluggable(arg);
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
		for (Connectors::const_iterator it =
			               pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			names.insert((*it)->getName());
		}
		for (Pluggables::const_iterator it =
			               pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			names.insert((*it)->getName());
		}
		completeString(tokens, names);
	}
}

} // namespace openmsx
