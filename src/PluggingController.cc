// $Id$

#include <cassert>
#include "ConsoleSource/ConsoleManager.hh"
#include "ConsoleSource/CommandController.hh"
#include "cpu/MSXCPU.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "openmsx.hh"


PluggingController::PluggingController()
{
	CommandController::instance()->registerCommand(plugCmd,   "plug");
	CommandController::instance()->registerCommand(plugCmd,   "insert");
	CommandController::instance()->registerCommand(unplugCmd, "unplug");
	CommandController::instance()->registerCommand(unplugCmd, "remove");
}

PluggingController::~PluggingController()
{
	CommandController::instance()->unregisterCommand("plug");
	CommandController::instance()->unregisterCommand("insert");
	CommandController::instance()->unregisterCommand("unplug");
	CommandController::instance()->unregisterCommand("remove");
}

PluggingController* PluggingController::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new PluggingController();
	}
	return oneInstance;
}
PluggingController* PluggingController::oneInstance = NULL;


void PluggingController::registerConnector(Connector *connector)
{
	connectors.push_back(connector);
}

void PluggingController::unregisterConnector(Connector *connector)
{
	std::vector<Connector*>::iterator i;
	for (i=connectors.begin(); i!=connectors.end(); i++) {
		if (*i == connector) {
			connectors.erase(i);
			return;
		}
	}
	assert(false);
}


void PluggingController::registerPluggable(Pluggable *pluggable)
{
	pluggables.push_back(pluggable);
}

void PluggingController::unregisterPluggable(Pluggable *pluggable)
{
	std::vector<Pluggable*>::iterator i;
	for (i=pluggables.begin(); i!=pluggables.end(); i++) {
		if (*i == pluggable) {
			pluggables.erase(i);
			return;
		}
	}
	assert(false);
}

// === Commands ===
//  plug command

void PluggingController::PlugCmd::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size()!=3) {
		ConsoleManager::instance()->print("Syntax error");
		return;
	}
	PluggingController* controller = PluggingController::instance();
	Connector* connector = NULL;
	std::vector<Connector*>::iterator i;
	for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
		if ((*i)->getName() == tokens[1]) {
			connector = *i;
			break;
		}
	}
	if (connector == NULL) {
		ConsoleManager::instance()->print("No such connector");
		return;
	}
	Pluggable* pluggable = NULL;
	std::vector<Pluggable*>::iterator j;
	for (j=controller->pluggables.begin(); j!=controller->pluggables.end(); j++) {
		if ((*j)->getName() == tokens[2]) {
			pluggable = *j;
			break;
		}
	}
	if (pluggable == NULL) {
		ConsoleManager::instance()->print("No such pluggable");
		return;
	}
	if (connector->getClass() != pluggable->getClass()) {
		ConsoleManager::instance()->print("Doesn't fit");
		return;
	}
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	connector->unplug(time);
	connector->plug(pluggable, time);

	PRT_DEBUG("Plug: " << connector->getName() << " " << pluggable->getName());
}

void PluggingController::PlugCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("Plugs a plug into a connector");
	ConsoleManager::instance()->print(" plug [connector] [plug]");
}

void PluggingController::PlugCmd::tabCompletion(std::vector<std::string> &tokens)
{
	PluggingController* controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		std::list<std::string> connectors;
		std::vector<Connector*>::iterator i;
		for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
			connectors.push_back((*i)->getName());
		}
		CommandController::completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		std::list<std::string> pluggables;
		std::vector<Pluggable*>::iterator i;
		for (i=controller->pluggables.begin(); i!=controller->pluggables.end(); i++) {
			pluggables.push_back((*i)->getName());
		}
		CommandController::completeString(tokens, pluggables);
	}
}


//  unplug command

void PluggingController::UnplugCmd::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size()!=2) {
		ConsoleManager::instance()->print("Syntax error");
		return;
	}
	PluggingController* controller = PluggingController::instance();
	Connector* connector = NULL;
	std::vector<Connector*>::iterator i;
	for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
		if ((*i)->getName() == tokens[1]) {
			connector = *i;
			break;
		}
	}
	if (connector == NULL) {
		ConsoleManager::instance()->print("No such connector");
		return;
	}
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	connector->unplug(time);
}

void PluggingController::UnplugCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("Unplugs a plug from a connector");
	ConsoleManager::instance()->print(" unplug [connector]");
}

void PluggingController::UnplugCmd::tabCompletion(std::vector<std::string> &tokens)
{
	PluggingController* controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		std::list<std::string> connectors;
		std::vector<Connector*>::iterator i;
		for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
			connectors.push_back((*i)->getName());
		}
		CommandController::completeString(tokens, connectors);
	}
}
