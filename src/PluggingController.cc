// $Id$

#include <cassert>
#include "ConsoleSource/Console.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"


PluggingController::PluggingController()
{
	Console::instance()->registerCommand(plugCmd, "plug");
	Console::instance()->registerCommand(plugCmd, "insert");
	Console::instance()->registerCommand(unplugCmd, "unplug");
	Console::instance()->registerCommand(unplugCmd, "remove");
}

PluggingController::~PluggingController()
{
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


void PluggingController::PlugCmd::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size()!=3) {
		Console::instance()->print("Syntax error");
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
		Console::instance()->print("No such connector");
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
		Console::instance()->print("No such pluggable");
		return;
	}
	if (connector->getClass() != pluggable->getClass()) {
		Console::instance()->print("Doesn't fit");
		return;
	}
	connector->unplug();
	connector->plug(pluggable);
}

void PluggingController::PlugCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("plug...");	// TODO
}

void PluggingController::UnplugCmd::execute(const std::vector<std::string> &tokens)
{
	if (tokens.size()!=2) {
		Console::instance()->print("Syntax error");
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
		Console::instance()->print("No such connector");
		return;
	}
	connector->unplug();
}

void PluggingController::UnplugCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("unplug...");	// TODO
}
