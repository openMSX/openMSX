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
#include "memory.hh"
#include <cassert>
#include <iostream>
#include <set>

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


PluggingController::PluggingController(MSXMotherBoard& motherBoard_)
	: plugCmd(make_unique<PlugCmd>(
		motherBoard_.getCommandController(),
		motherBoard_.getStateChangeDistributor(),
		motherBoard_.getScheduler(), *this))
	, unplugCmd(make_unique<UnplugCmd>(
		motherBoard_.getCommandController(),
		motherBoard_.getStateChangeDistributor(),
		motherBoard_.getScheduler(), *this))
	, pluggableInfo(make_unique<PluggableInfo>(
		motherBoard_.getMachineInfoCommand(), *this))
	, connectorInfo(make_unique<ConnectorInfo>(
		motherBoard_.getMachineInfoCommand(), *this))
	, connectionClassInfo(make_unique<ConnectionClassInfo>(
		motherBoard_.getMachineInfoCommand(), *this))
	, motherBoard(motherBoard_)
{
	PluggableFactory::createAll(*this, motherBoard);
}

PluggingController::~PluggingController()
{
#ifndef NDEBUG
	// This is similar to an assert: it should never print anything,
	// but if it does, it helps to catch an error.
	for (auto& c : connectors) {
		std::cerr << "ERROR: Connector still registered at shutdown: "
		          << c->getName() << std::endl;
	}
#endif
}

void PluggingController::registerConnector(Connector& connector)
{
	connectors.push_back(&connector);
	getCliComm().update(CliComm::CONNECTOR, connector.getName(), "add");
}

void PluggingController::unregisterConnector(Connector& connector)
{
	auto it = find(begin(connectors), end(connectors), &connector);
	assert(it != end(connectors));
	connectors.erase(it);

	getCliComm().update(CliComm::CONNECTOR, connector.getName(), "remove");
}


void PluggingController::registerPluggable(std::unique_ptr<Pluggable> pluggable)
{
	pluggables.push_back(std::move(pluggable));
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
		for (auto& c : pluggingController.connectors) {
			result << c->getName() << ": "
			       << c->getPlugged().getName() << '\n';
		}
		break;
	}
	case 2: {
		auto& connector = pluggingController.getConnector(tokens[1]);
		result << connector.getName() << ": "
		       << connector.getPlugged().getName();
		break;
	}
	case 3: {
		auto& connector = pluggingController.getConnector(tokens[1]);
		auto& pluggable = pluggingController.getPluggable(tokens[2]);
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
			pluggingController.getCliComm().update(
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
		vector<string_ref> connectors;
		for (auto& c : pluggingController.connectors) {
			connectors.push_back(c->getName());
		}
		completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		vector<string_ref> pluggables;
		auto* connector = pluggingController.findConnector(tokens[1]);
		string_ref className = connector ? connector->getClass() : "";
		for (auto& p : pluggingController.pluggables) {
			if (p->getClass() == className) {
				pluggables.push_back(p->getName());
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
	auto& connector = pluggingController.getConnector(tokens[1]);
	connector.unplug(time);
	pluggingController.getCliComm().update(CliComm::UNPLUG, tokens[1], "");
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
		vector<string_ref> connectors;
		for (auto& c : pluggingController.connectors) {
			connectors.push_back(c->getName());
		}
		completeString(tokens, connectors);
	}
}

Connector* PluggingController::findConnector(string_ref name) const
{
	for (auto& c : connectors) {
		if (c->getName() == name) {
			return c;
		}
	}
	return nullptr;
}

Connector& PluggingController::getConnector(string_ref name) const
{
	if (auto* result = findConnector(name)) {
		return *result;
	}
	throw CommandException("No such connector: " + name);
}

Pluggable* PluggingController::findPluggable(string_ref name) const
{
	for (auto& p : pluggables) {
		if (p->getName() == name) {
			return p.get();
		}
	}
	return nullptr;
}

Pluggable& PluggingController::getPluggable(string_ref name) const
{
	if (auto* result = findPluggable(name)) {
		return *result;
	}
	throw CommandException("No such pluggable: " + name);
}

CliComm& PluggingController::getCliComm()
{
	return motherBoard.getMSXCliComm();
}

EmuTime::param PluggingController::getCurrentTime() const
{
	return motherBoard.getCurrentTime();
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
		for (auto& p : pluggingController.pluggables) {
			result.addListElement(p->getName());
		}
		break;
	case 3: {
		auto& pluggable = pluggingController.getPluggable(
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
		vector<string_ref> pluggables;
		for (auto& p : pluggingController.pluggables) {
			pluggables.push_back(p->getName());
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
		for (auto& c : pluggingController.connectors) {
			result.addListElement(c->getName());
		}
		break;
	case 3: {
		auto& connector = pluggingController.getConnector(tokens[2].getString());
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
		vector<string_ref> connectors;
		for (auto& c : pluggingController.connectors) {
			connectors.push_back(c->getName());
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
		std::set<string_ref> classes; // filter duplicates
		for (auto& c : pluggingController.connectors) {
			classes.insert(c->getClass());
		}
		for (auto& p : pluggingController.pluggables) {
			classes.insert(p->getClass());
		}
		result.addListElements(classes);
		break;
	}
	case 3: {
		const auto& arg = tokens[2].getString();
		if (auto* connector = pluggingController.findConnector(arg)) {
			result.setString(connector->getClass());
			break;
		}
		if (auto* pluggable = pluggingController.findPluggable(arg)) {
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
		vector<string_ref> names;
		for (auto& c : pluggingController.connectors) {
			names.push_back(c->getName());
		}
		for (auto& p : pluggingController.pluggables) {
			names.push_back(p->getName());
		}
		completeString(tokens, names);
	}
}

} // namespace openmsx
