// $Id$

#include <cassert>
#include "CommandController.hh"
#include "MSXCPU.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include "openmsx.hh"


PluggingController::PluggingController()
{
	CommandController::instance()->registerCommand(&plugCmd,   "plug");
	CommandController::instance()->registerCommand(&unplugCmd, "unplug");
}

PluggingController::~PluggingController()
{
	CommandController::instance()->unregisterCommand(&plugCmd,   "plug");
	CommandController::instance()->unregisterCommand(&unplugCmd, "unplug");
}

PluggingController* PluggingController::instance()
{
	static PluggingController* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new PluggingController();
	return oneInstance;
}


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
}

// === Commands ===
//  plug command

void PluggingController::PlugCmd::execute(const std::vector<std::string> &tokens,
                                          const EmuTime &time)
{
	PluggingController* controller = PluggingController::instance();
	switch (tokens.size()) {
		case 1: { 
			std::vector<Connector*>::iterator i;
			for (i =  controller->connectors.begin();
			     i != controller->connectors.end();
			     i++) {
				print((*i)->getName() + ": " + 
				      (*i)->getPlug()->getName());
			}
			break;
		}
		case 2: {
			Connector* connector = NULL;
			std::vector<Connector*>::iterator i;
			for (i =  controller->connectors.begin();
			     i != controller->connectors.end();
			     i++) {
				if ((*i)->getName() == tokens[1]) {
					connector = *i;
					break;
				}
			}
			if (connector == NULL)
				throw CommandException("No such connector");
			print(connector->getName() + ": " + 
			      connector->getPlug()->getName());
			break;
		}
		case 3: {
			Connector* connector = NULL;
			std::vector<Connector*>::iterator i;
			for (i =  controller->connectors.begin();
			     i != controller->connectors.end();
			     i++) {
				if ((*i)->getName() == tokens[1]) {
					connector = *i;
					break;
				}
			}
			if (connector == NULL)
				throw CommandException("No such connector");
			Pluggable* pluggable = NULL;
			std::vector<Pluggable*>::iterator j;
			for (j =  controller->pluggables.begin();
			     j != controller->pluggables.end();
			     j++) {
				if ((*j)->getName() == tokens[2]) {
					pluggable = *j;
					break;
				}
			}
			if (pluggable == NULL)
				throw CommandException("No such pluggable");
			if (connector->getClass() != pluggable->getClass())
				throw CommandException("Doesn't fit");
			connector->unplug(time);
			connector->plug(pluggable, time);
			break;
		}
	default:
		throw CommandException("Syntax error");
	}
}

void PluggingController::PlugCmd::help(const std::vector<std::string> &tokens) const
{
	print("Plugs a plug into a connector");
	print(" plug [connector] [plug]");
}

void PluggingController::PlugCmd::tabCompletion(std::vector<std::string> &tokens) const
{
	PluggingController* controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		std::set<std::string> connectors;
		std::vector<Connector*>::iterator i;
		for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
			connectors.insert((*i)->getName());
		}
		CommandController::completeString(tokens, connectors);
	} else if (tokens.size() == 3) {
		// complete pluggable
		std::set<std::string> pluggables;
		std::vector<Pluggable*>::iterator i;
		for (i=controller->pluggables.begin(); i!=controller->pluggables.end(); i++) {
			pluggables.insert((*i)->getName());
		}
		CommandController::completeString(tokens, pluggables);
	}
}


//  unplug command

void PluggingController::UnplugCmd::execute(const std::vector<std::string> &tokens,
                                            const EmuTime &time)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	PluggingController* controller = PluggingController::instance();
	Connector* connector = NULL;
	std::vector<Connector*>::iterator i;
	for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
		if ((*i)->getName() == tokens[1]) {
			connector = *i;
			break;
		}
	}
	if (connector == NULL)
		throw CommandException("No such connector");
	connector->unplug(time);
}

void PluggingController::UnplugCmd::help(const std::vector<std::string> &tokens) const
{
	print("Unplugs a plug from a connector");
	print(" unplug [connector]");
}

void PluggingController::UnplugCmd::tabCompletion(std::vector<std::string> &tokens) const
{
	PluggingController* controller = PluggingController::instance();
	if (tokens.size() == 2) {
		// complete connector
		std::set<std::string> connectors;
		std::vector<Connector*>::iterator i;
		for (i=controller->connectors.begin(); i!=controller->connectors.end(); i++) {
			connectors.insert((*i)->getName());
		}
		CommandController::completeString(tokens, connectors);
	}
}
