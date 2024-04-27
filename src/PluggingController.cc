#include "PluggingController.hh"
#include "PlugException.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "PluggableFactory.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "MSXCliComm.hh"
#include "outer.hh"
#include "ranges.hh"
#include "view.hh"
#include <iostream>

using std::string;
using std::string_view;

namespace openmsx {

PluggingController::PluggingController(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, plugCmd(
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler())
	, unplugCmd(
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler())
	, pluggableInfo      (motherBoard.getMachineInfoCommand())
	, connectorInfo      (motherBoard.getMachineInfoCommand())
	, connectionClassInfo(motherBoard.getMachineInfoCommand())
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
		          << c->getName() << '\n';
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
	connector.unplug(motherBoard.getCurrentTime());
	move_pop_back(connectors, rfind_unguarded(connectors, &connector));
	getCliComm().update(CliComm::CONNECTOR, connector.getName(), "remove");
}


void PluggingController::registerPluggable(std::unique_ptr<Pluggable> pluggable)
{
	pluggables.push_back(std::move(pluggable));
}


// === Commands ===
//  plug command

PluggingController::PlugCmd::PlugCmd(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "plug")
{
}

void PluggingController::PlugCmd::execute(
	std::span<const TclObject> tokens, TclObject& result_, EmuTime::param time)
{
	checkNumArgs(tokens, Between{1, 3}, Prefix{1}, "?connector? ?pluggable?");
	string result;
	auto& pluggingController = OUTER(PluggingController, plugCmd);
	switch (tokens.size()) {
	case 1:
		for (auto& c : pluggingController.connectors) {
			strAppend(result, c->getName(), ": ",
			          c->getPlugged().getName(), '\n');
		}
		break;
	case 2: {
		const auto& connector = pluggingController.getConnector(tokens[1].getString());
		strAppend(result, connector.getName(), ": ",
		          connector.getPlugged().getName());
		break;
	}
	case 3: {
		string_view connName = tokens[1].getString();
		string_view plugName = tokens[2].getString();
		auto& connector = pluggingController.getConnector(connName);
		auto& pluggable = pluggingController.getPluggable(plugName);
		if (&connector.getPlugged() == &pluggable) {
			// already plugged, don't unplug/replug
			break;
		}
		if (connector.getClass() != pluggable.getClass()) {
			throw CommandException("plug: ", plugName,
			                       " doesn't fit in ", connName);
		}
		connector.unplug(time);
		try {
			connector.plug(pluggable, time);
			pluggingController.getCliComm().update(
				CliComm::PLUG, connName, plugName);
		} catch (PlugException& e) {
			throw CommandException("plug: plug failed: ", e.getMessage());
		}
		break;
	}
	}
	result_ = result; // TODO return Tcl list
}

string PluggingController::PlugCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Plugs a plug into a connector\n"
	       " plug [connector] [plug]";
}

void PluggingController::PlugCmd::tabCompletion(std::vector<string>& tokens) const
{
	auto& pluggingController = OUTER(PluggingController, plugCmd);
	if (tokens.size() == 2) {
		// complete connector
		completeString(tokens, view::transform(
			pluggingController.connectors,
			[](auto& c) -> std::string_view { return c->getName(); }));
	} else if (tokens.size() == 3) {
		// complete pluggable
		const auto* connector = pluggingController.findConnector(tokens[1]);
		string_view className = connector ? connector->getClass() : string_view{};
		completeString(tokens, view::transform(view::filter(pluggingController.pluggables,
			[&](auto& p) { return p->getClass() == className; }),
			[](auto& p) -> string_view { return p->getName(); }));
	}
}

bool PluggingController::PlugCmd::needRecord(std::span<const TclObject> tokens) const
{
	return tokens.size() == 3;
}


//  unplug command

PluggingController::UnplugCmd::UnplugCmd(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "unplug")
{
}

void PluggingController::UnplugCmd::execute(
	std::span<const TclObject> tokens, TclObject& /*result*/, EmuTime::param time)
{
	checkNumArgs(tokens, 2, "connector");
	auto& pluggingController = OUTER(PluggingController, unplugCmd);
	string_view connName = tokens[1].getString();
	auto& connector = pluggingController.getConnector(connName);
	connector.unplug(time);
	pluggingController.getCliComm().update(CliComm::PLUG, connName, {});
}

string PluggingController::UnplugCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Unplugs a plug from a connector\n"
	       " unplug [connector]";
}

void PluggingController::UnplugCmd::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete connector
		completeString(tokens, view::transform(
			OUTER(PluggingController, unplugCmd).connectors,
			[](auto* c) -> std::string_view { return c->getName(); }));
	}
}

Connector* PluggingController::findConnector(string_view name) const
{
	auto it = ranges::find(connectors, name, &Connector::getName);
	return (it != end(connectors)) ? *it : nullptr;
}

Connector& PluggingController::getConnector(string_view name) const
{
	if (auto* result = findConnector(name)) {
		return *result;
	}
	throw CommandException("No such connector: ", name);
}

Pluggable* PluggingController::findPluggable(string_view name) const
{
	auto it = ranges::find(pluggables, name, &Pluggable::getName);
	return (it != end(pluggables)) ? it->get() : nullptr;
}

Pluggable& PluggingController::getPluggable(string_view name) const
{
	if (auto* result = findPluggable(name)) {
		return *result;
	}
	throw CommandException("No such pluggable: ", name);
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

PluggingController::PluggableInfo::PluggableInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "pluggable")
{
}

void PluggingController::PluggableInfo::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	auto& pluggingController = OUTER(PluggingController, pluggableInfo);
	switch (tokens.size()) {
	case 2:
		result.addListElements(
			view::transform(pluggingController.pluggables,
			                [](auto& p) { return p->getName(); }));
		break;
	case 3: {
		const auto& pluggable = pluggingController.getPluggable(
				tokens[2].getString());
		result = pluggable.getDescription();
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::PluggableInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Shows a list of available pluggables. "
	       "Or show info on a specific pluggable.";
}

void PluggingController::PluggableInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			OUTER(PluggingController, pluggableInfo).pluggables,
			[](auto& p) -> std::string_view { return p->getName(); }));
	}
}

// Connector info

PluggingController::ConnectorInfo::ConnectorInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "connector")
{
}

void PluggingController::ConnectorInfo::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	auto& pluggingController = OUTER(PluggingController, connectorInfo);
	switch (tokens.size()) {
	case 2:
		result.addListElements(
			view::transform(pluggingController.connectors,
			                [](auto& c) { return c->getName(); }));
		break;
	case 3: {
		const auto& connector = pluggingController.getConnector(tokens[2].getString());
		result = connector.getDescription();
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::ConnectorInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Shows a list of available connectors.";
}

void PluggingController::ConnectorInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			OUTER(PluggingController, connectorInfo).connectors,
			[](auto& c) -> std::string_view { return c->getName(); }));
	}
}

// Connection Class info

PluggingController::ConnectionClassInfo::ConnectionClassInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "connectionclass")
{
}

void PluggingController::ConnectionClassInfo::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	const auto& pluggingController = OUTER(PluggingController, connectionClassInfo);
	switch (tokens.size()) {
	case 2: {
		std::vector<string_view> classes;
		classes.reserve(pluggingController.connectors.size());
		for (auto& c : pluggingController.connectors) {
			classes.push_back(c->getClass());
		}
		for (const auto& p : pluggingController.pluggables) {
			auto c = p->getClass();
			if (!contains(classes, c)) classes.push_back(c);
		}
		result.addListElements(classes);
		break;
	}
	case 3: {
		const auto& arg = tokens[2].getString();
		if (const auto* connector = pluggingController.findConnector(arg)) {
			result = connector->getClass();
			break;
		}
		if (const auto* pluggable = pluggingController.findPluggable(arg)) {
			result = pluggable->getClass();
			break;
		}
		throw CommandException("No such connector or pluggable");
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string PluggingController::ConnectionClassInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Shows the class a connector or pluggable belongs to.";
}

void PluggingController::ConnectionClassInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		auto& pluggingController = OUTER(PluggingController, connectionClassInfo);
		auto names = concat(
			view::transform(pluggingController.connectors,
			                [](auto& c) -> std::string_view { return c->getName(); }),
			view::transform(pluggingController.pluggables,
			                [](auto& p) -> std::string_view { return p->getName(); }));
		completeString(tokens, names);
	}
}

} // namespace openmsx
