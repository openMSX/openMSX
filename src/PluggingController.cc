// $Id$

#include "PluggingController.hh"
#include "PlugException.hh"
#include "RecordedCommand.hh"
#include "InfoTopic.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "PluggableFactory.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include <cassert>
#include <iostream>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class PlugCmd : public RecordedCommand
{
public:
	PlugCmd(CommandController& commandController,
	        StateChangeDistributor& stateChangeDistributor,
	        Scheduler& scheduler,
	        PluggingController& pluggingController);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
	virtual bool needRecord(const vector<string>& tokens) const;
private:
	PluggingController& pluggingController;
};

class UnplugCmd : public RecordedCommand
{
public:
	UnplugCmd(CommandController& commandController,
	          StateChangeDistributor& stateChangeDistributor,
	          Scheduler& scheduler,
	          PluggingController& pluggingController);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	PluggingController& pluggingController;
};

class PluggableInfo : public InfoTopic
{
public:
	PluggableInfo(InfoCommand& machineInfoCommand,
		      PluggingController& pluggingController);
	virtual void execute(const vector<TclObject>& tokens,
			     TclObject& result) const;
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	PluggingController& pluggingController;
};

class ConnectorInfo : public InfoTopic
{
public:
	ConnectorInfo(InfoCommand& machineInfoCommand,
		      PluggingController& pluggingController);
	virtual void execute(const vector<TclObject>& tokens,
			     TclObject& result) const;
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	PluggingController& pluggingController;
};

class ConnectionClassInfo : public InfoTopic
{
public:
	ConnectionClassInfo(InfoCommand& machineInfoCommand,
			    PluggingController& pluggingController);
	virtual void execute(const vector<TclObject>& tokens,
			     TclObject& result) const;
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	PluggingController& pluggingController;
};


PluggingController::PluggingController(MSXMotherBoard& motherBoard)
	: plugCmd  (new PlugCmd  (motherBoard.getCommandController(),
	                          motherBoard.getStateChangeDistributor(),
	                          motherBoard.getScheduler(), *this))
	, unplugCmd(new UnplugCmd(motherBoard.getCommandController(),
	                          motherBoard.getStateChangeDistributor(),
	                          motherBoard.getScheduler(), *this))
	, pluggableInfo(new PluggableInfo(
		motherBoard.getMachineInfoCommand(), *this))
	, connectorInfo(new ConnectorInfo(
		motherBoard.getMachineInfoCommand(), *this))
	, connectionClassInfo(new ConnectionClassInfo(
		motherBoard.getMachineInfoCommand(), *this))
	, cliComm(motherBoard.getMSXCliComm())
{
	PluggableFactory::createAll(*this, motherBoard);
}

PluggingController::~PluggingController()
{
	for (Pluggables::iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		delete *it;
	}
#ifndef NDEBUG
	// This is similar to an assert: it should never print anything,
	// but if it does, it helps to catch an error.
	for (Connectors::const_iterator it = connectors.begin();
	     it != connectors.end(); ++it) {
		std::cerr << "ERROR: Connector still registered at shutdown: "
		          << (*it)->getName() << std::endl;
	}
#endif
}

void PluggingController::registerConnector(Connector& connector)
{
	connectors.push_back(&connector);
	cliComm.update(CliComm::CONNECTOR, connector.getName(), "add");
}

void PluggingController::unregisterConnector(Connector& connector)
{
	Connectors::iterator it = find(connectors.begin(), connectors.end(),
	                               &connector);
	assert(it != connectors.end());
	connectors.erase(it);

	cliComm.update(CliComm::CONNECTOR, connector.getName(), "remove");
}


void PluggingController::registerPluggable(std::auto_ptr<Pluggable> pluggable)
{
	pluggables.push_back(pluggable.release());
}


// === Commands ===
//  plug command

PlugCmd::PlugCmd(CommandController& commandController,
                 StateChangeDistributor& stateChangeDistributor,
                 Scheduler& scheduler,
                 PluggingController& pluggingController_)
	: RecordedCommand(commandController, stateChangeDistributor,
	                  scheduler, "plug")
	, pluggingController(pluggingController_)
{
}

string PlugCmd::execute(const vector<string>& tokens, EmuTime::param time)
{
	StringOp::Builder result;
	switch (tokens.size()) {
	case 1: {
		for (PluggingController::Connectors::const_iterator it =
		                       pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			result << (*it)->getName() << ": "
			       << (*it)->getPlugged().getName() << '\n';
		}
		break;
	}
	case 2: {
		Connector& connector = pluggingController.getConnector(tokens[1]);
		result << connector.getName() << ": "
		       << connector.getPlugged().getName();
		break;
	}
	case 3: {
		Connector& connector = pluggingController.getConnector(tokens[1]);
		Pluggable& pluggable = pluggingController.getPluggable(tokens[2]);
		if (&connector.getPlugged() == &pluggable) {
			// already plugged, don't unplug/replug
			break;
		}
		if (connector.getClass() != pluggable.getClass()) {
			throw CommandException("plug: " + tokens[2] +
					   " doesn't fit in " + tokens[1]);
		}
		connector.unplug(time);
		try {
			connector.plug(pluggable, time);
			pluggingController.cliComm.update(
				CliComm::PLUG, tokens[1], tokens[2]);
		} catch (PlugException& e) {
			throw CommandException("plug: plug failed: " + e.getMessage());
		}
		break;
	}
	default:
		throw SyntaxError();
	}
	return result;
}

string PlugCmd::help(const vector<string>& /*tokens*/) const
{
	return "Plugs a plug into a connector\n"
	       " plug [connector] [plug]";
}

void PlugCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (PluggingController::Connectors::const_iterator it =
		                       pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		set<string> pluggables;
		Connector* connector = pluggingController.findConnector(tokens[1]);
		string_ref className = connector ? connector->getClass() : "";
		for (PluggingController::Pluggables::const_iterator it =
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

bool PlugCmd::needRecord(const vector<string>& tokens) const
{
	return tokens.size() == 3;
}


//  unplug command

UnplugCmd::UnplugCmd(CommandController& commandController,
                     StateChangeDistributor& stateChangeDistributor,
                     Scheduler& scheduler,
                     PluggingController& pluggingController_)
	: RecordedCommand(commandController, stateChangeDistributor,
	                  scheduler, "unplug")
	, pluggingController(pluggingController_)
{
}

string UnplugCmd::execute(const vector<string>& tokens, EmuTime::param time)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	Connector& connector = pluggingController.getConnector(tokens[1]);
	connector.unplug(time);
	pluggingController.cliComm.update(CliComm::UNPLUG, tokens[1], "");
	return "";
}

string UnplugCmd::help(const vector<string>& /*tokens*/) const
{
	return "Unplugs a plug from a connector\n"
	       " unplug [connector]";
}

void UnplugCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete connector
		set<string> connectors;
		for (PluggingController::Connectors::const_iterator it =
		                       pluggingController.connectors.begin();
		     it != pluggingController.connectors.end();
		     ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	}
}

Connector* PluggingController::findConnector(string_ref name) const
{
	for (Connectors::const_iterator it = connectors.begin();
	     it != connectors.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

Connector& PluggingController::getConnector(string_ref name) const
{
	Connector* result = findConnector(name);
	if (!result) {
		throw CommandException("No such connector: " + name);
	}
	return *result;
}

Pluggable* PluggingController::findPluggable(string_ref name) const
{
	for (Pluggables::const_iterator it = pluggables.begin();
	     it != pluggables.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

Pluggable& PluggingController::getPluggable(string_ref name) const
{
	Pluggable* result = findPluggable(name);
	if (!result) {
		throw CommandException("No such pluggable: " + name);
	}
	return *result;
}

CliComm& PluggingController::getCliComm()
{
	return cliComm;
}


// Pluggable info

PluggableInfo::PluggableInfo(InfoCommand& machineInfoCommand,
                             PluggingController& pluggingController_)
	: InfoTopic(machineInfoCommand, "pluggable")
	, pluggingController(pluggingController_)
{
}

void PluggableInfo::execute(const vector<TclObject>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (PluggingController::Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Pluggable& pluggable = pluggingController.getPluggable(
				tokens[2].getString());
		result.setString(pluggable.getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggableInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available pluggables. "
	       "Or show info on a specific pluggable.";
}

void PluggableInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> pluggables;
		for (PluggingController::Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			pluggables.insert((*it)->getName());
		}
		completeString(tokens, pluggables);
	}
}

// Connector info

ConnectorInfo::ConnectorInfo(InfoCommand& machineInfoCommand,
                             PluggingController& pluggingController_)
	: InfoTopic(machineInfoCommand, "connector")
	, pluggingController(pluggingController_)
{
}

void ConnectorInfo::execute(const vector<TclObject>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (PluggingController::Connectors::const_iterator it =
			 pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		const Connector& connector = pluggingController.getConnector(tokens[2].getString());
		result.setString(connector.getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string ConnectorInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available connectors.";
}

void ConnectorInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> connectors;
		for (PluggingController::Connectors::const_iterator it =
		                      pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			connectors.insert((*it)->getName());
		}
		completeString(tokens, connectors);
	}
}

// Connection Class info

ConnectionClassInfo::ConnectionClassInfo(
		InfoCommand& machineInfoCommand,
		PluggingController& pluggingController_)
	: InfoTopic(machineInfoCommand, "connectionclass")
	, pluggingController(pluggingController_)
{
}

void ConnectionClassInfo::execute(const vector<TclObject>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string_ref> classes;
		for (PluggingController::Connectors::const_iterator it =
			 pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		for (PluggingController::Pluggables::const_iterator it =
			 pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			classes.insert((*it)->getClass());
		}
		result.addListElements(classes.begin(), classes.end());
		break;
	}
	case 3: {
		string_ref arg = tokens[2].getString();
		if (const Connector* connector =
		    pluggingController.findConnector(arg)) {
			result.setString(connector->getClass());
			break;
		}
		if (const Pluggable* pluggable =
		    pluggingController.findPluggable(arg)) {
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

string ConnectionClassInfo::help(const vector<string>& /*tokens*/) const
{
	return "Shows the class a connector or pluggable belongs to.";
}

void ConnectionClassInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> names;
		for (PluggingController::Connectors::const_iterator it =
			               pluggingController.connectors.begin();
		     it != pluggingController.connectors.end(); ++it) {
			names.insert((*it)->getName());
		}
		for (PluggingController::Pluggables::const_iterator it =
			               pluggingController.pluggables.begin();
		     it != pluggingController.pluggables.end(); ++it) {
			names.insert((*it)->getName());
		}
		completeString(tokens, names);
	}
}

} // namespace openmsx
