// $Id$

#include <cassert>
#include <cstdio>
#include "CommandController.hh"
#include "MSXCPU.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "PluggableFactory.hh"
#include "openmsx.hh"
#include "InfoCommand.hh"


namespace openmsx {

PluggingController::PluggingController()
{
	PluggableFactory::createAll(this);

	CommandController::instance()->registerCommand(&plugCmd,   "plug");
	CommandController::instance()->registerCommand(&unplugCmd, "unplug");
	InfoCommand::instance().registerTopic("pluggable", &pluggableInfo);
	InfoCommand::instance().registerTopic("connector", &connectorInfo);
}

PluggingController::~PluggingController()
{
	CommandController::instance()->unregisterCommand(&plugCmd,   "plug");
	CommandController::instance()->unregisterCommand(&unplugCmd, "unplug");
	InfoCommand::instance().unregisterTopic("pluggable", &pluggableInfo);
	InfoCommand::instance().unregisterTopic("connector", &connectorInfo);

#ifndef NDEBUG
	// This is similar to an assert: it should never print anything,
	// but if it does, it helps to catch an error.
	for (vector<Connector *>::const_iterator it = connectors.begin();
		it != connectors.end(); ++it
	) {
		fprintf(stderr,
			"ERROR: Connector still plugged at shutdown: %s\n",
			(*it)->getName().c_str()
			);
	}
#endif
	for (vector<Pluggable *>::iterator it = pluggables.begin();
		it != pluggables.end(); ++it
	) {
		delete (*it);
	}
}

PluggingController *PluggingController::instance()
{
	static PluggingController oneInstance;
	return &oneInstance;
}

void PluggingController::registerConnector(Connector *connector)
{
	connectors.push_back(connector);
}

void PluggingController::unregisterConnector(Connector *connector)
{
	for (vector<Connector *>::iterator it = connectors.begin();
	     it != connectors.end();
	     ++it) {
		if ((*it) == connector) {
			connectors.erase(it);
			return;
		}
	}
}


void PluggingController::registerPluggable(Pluggable *pluggable)
{
	pluggables.push_back(pluggable);
}

void PluggingController::unregisterPluggable(Pluggable *pluggable)
{
	for (vector<Pluggable *>::iterator it = pluggables.begin();
	     it != pluggables.end();
	     ++it) {
		if ((*it) == pluggable) {
			pluggables.erase(it);
			return;
		}
	}
}

// === Commands ===
//  plug command

string PluggingController::PlugCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	PluggingController *controller = PluggingController::instance();
	switch (tokens.size()) {
		case 1: {
			for (vector<Connector *>::const_iterator it =
			                       controller->connectors.begin();
			     it != controller->connectors.end();
			     ++it) {
				result += ((*it)->getName() + ": " +
				           (*it)->getPlugged()->getName()) + '\n';
			}
			break;
		}
		case 2: {
			Connector *connector = controller->getConnector(tokens[1]);
			if (connector == NULL) {
				throw CommandException("plug: " + tokens[1] + ": no such connector");
			}
			result += (connector->getName() + ": " +
			           connector->getPlugged()->getName()) + '\n';
			break;
		}
		case 3: {
			Connector *connector = controller->getConnector(tokens[1]);
			if (connector == NULL) {
				throw CommandException("plug: " + tokens[1] + ": no such connector");
			}
			Pluggable *pluggable = controller->getPluggable(tokens[2]);
			if (pluggable == NULL) {
				throw CommandException("plug: " + tokens[2] + ": no such pluggable");
			}
			if (connector->getClass() != pluggable->getClass()) {
				throw CommandException("plug: " + tokens[2] + " doesn't fit in " + tokens[1]);
			}
			connector->unplug(time);
			try {
				connector->plug(pluggable, time);
			} catch (PlugException &e) {
				throw CommandException("plug: plug failed: " + e.getMessage());
			}
			break;
		}
	default:
		throw CommandException("plug: syntax error");
	}
	return result;
}

string PluggingController::PlugCmd::help(const vector<string> &tokens) const
	throw()
{
	return "Plugs a plug into a connector\n"
	       " plug [connector] [plug]\n";
}

void PluggingController::PlugCmd::tabCompletion(vector<string> &tokens) const
	throw()
{
	PluggingController *controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (vector<Connector*>::const_iterator it =
			               controller->connectors.begin();
		     it != controller->connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		CommandController::completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		set<string> pluggables;
		Connector* connector = controller->getConnector(tokens[1]);
		string className = connector ? connector->getClass() : "";
		for (vector<Pluggable*>::const_iterator it =
			 controller->pluggables.begin();
		     it != controller->pluggables.end(); ++it) {
			Pluggable* pluggable = *it;
			if (pluggable->getClass() == className) {
				pluggables.insert(pluggable->getName());
			}
		}
		CommandController::completeString(tokens, pluggables);
	}
}


//  unplug command

string PluggingController::UnplugCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	if (tokens.size() != 2) {
		throw CommandException("Syntax error");
	}
	PluggingController *controller = PluggingController::instance();
	Connector *connector = controller->getConnector(tokens[1]);
	if (connector == NULL) {
		throw CommandException("No such connector");
	}
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	connector->unplug(time);
	return "";
}

string PluggingController::UnplugCmd::help(const vector<string> &tokens) const
	throw()
{
	return "Unplugs a plug from a connector\n"
	       " unplug [connector]\n";
}

void PluggingController::UnplugCmd::tabCompletion(vector<string> &tokens) const
	throw()
{
	PluggingController *controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (vector<Connector *>::const_iterator it =
		                       controller->connectors.begin();
		     it != controller->connectors.end();
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

string PluggingController::PluggableInfo::execute(const vector<string> &tokens)
	const throw(CommandException)
{
	string result;
	PluggingController* controller = PluggingController::instance();
	switch (tokens.size()) {
	case 2:
		for (vector<Pluggable*>::const_iterator it =
			 controller->pluggables.begin();
		     it != controller->pluggables.end(); ++it) {
			result += (*it)->getName() + '\n';
		}
		break;
	case 3:
		result = controller->getPluggable(tokens[2])->getDescription();
		break;
	default:
		throw CommandException("Too many parameters");
	}
	return result;
}

string PluggingController::PluggableInfo::help(const vector<string> &tokens) const
	throw()
{
	return "Shows a list of available pluggables. "
	       "Or show info on a specific pluggable.\n";
}

void PluggingController::PluggableInfo::tabCompletion(vector<string> &tokens) const
	throw()
{
	if (tokens.size() == 3) {
		PluggingController* controller = PluggingController::instance();
		set<string> pluggables;
		for (vector<Pluggable*>::const_iterator it =
			 controller->pluggables.begin();
		     it != controller->pluggables.end(); ++it) {
			pluggables.insert((*it)->getName());
		}
		CommandController::completeString(tokens, pluggables);
	}
}

// Connector info

string PluggingController::ConnectorInfo::execute(const vector<string> &tokens)
	const throw(CommandException)
{
	string result;
	PluggingController* controller = PluggingController::instance();
	switch (tokens.size()) {
	case 2:
		for (vector<Connector*>::const_iterator it =
			 controller->connectors.begin();
		     it != controller->connectors.end(); ++it) {
			result += (*it)->getName() + '\n';
		}
		break;
	case 3:
		result = controller->getConnector(tokens[2])->getDescription();
		break;
	default:
		throw CommandException("Too many parameters");
	}
	return result;
}

string PluggingController::ConnectorInfo::help(const vector<string> &tokens) const
	throw()
{
	return "Shows a list of available connectors.\n";
}

void PluggingController::ConnectorInfo::tabCompletion(vector<string> &tokens) const
	throw()
{
	if (tokens.size() == 3) {
		PluggingController* controller = PluggingController::instance();
		set<string> connectors;
		for (vector<Connector*>::const_iterator it =
			               controller->connectors.begin();
		     it != controller->connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		CommandController::completeString(tokens, connectors);
	}
}

} // namespace openmsx
