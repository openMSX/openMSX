// $Id$

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "CommandController.hh"
#include "CliCommOutput.hh"
#include "openmsx.hh"
#include "config.h"
#include "Scheduler.hh"
#include "HotKey.hh"
#include "MSXCPU.hh"

using std::ostringstream;

namespace openmsx {

EventDistributor::EventDistributor()
	: grabInput("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false),
	  lastAfterId(0)
{
	// Make sure HotKey is instantiated
	HotKey::instance();	// TODO is there a better place for this?
	grabInput.addListener(this);
	CommandController::instance()->registerCommand(&quitCommand, "quit");
	CommandController::instance()->registerCommand(&afterCommand, "after");
}

EventDistributor::~EventDistributor()
{
	CommandController::instance()->unregisterCommand(&afterCommand, "after");
	CommandController::instance()->unregisterCommand(&quitCommand, "quit");
	grabInput.removeListener(this);
}

EventDistributor *EventDistributor::instance()
{
	static EventDistributor* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}

void EventDistributor::wait()
{
	if (SDL_WaitEvent(NULL)) {
		poll();
	}
}

void EventDistributor::poll()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
		PRT_DEBUG("SDL event received");
		if (event.type == SDL_QUIT) {
			quit();
		} else {
			handle(event);
		}
	}
}

void EventDistributor::notify()
{
	static SDL_Event event;
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);
}

void EventDistributor::quit()
{
	Scheduler::instance()->stopScheduling();
}

void EventDistributor::handle(SDL_Event &event)
{
	for (map<unsigned, AfterCmd*>::const_iterator it = afterCmds.begin();
	     it != afterCmds.end(); ++it) {
		AfterCmd* cmd = it->second;
		if (cmd->type == IDLE) {
			Scheduler::instance()->removeSyncPoint(cmd);
			EmuTime t = MSXCPU::instance()->getCurrentTime() +
			            EmuDuration(cmd->time);
			Scheduler::instance()->setSyncPoint(t, cmd);
		}
	}
	
	bool cont = true;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds;
	bounds = highMap.equal_range(event.type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		cont &= it->second->signalEvent(event);
	}
	if (!cont) {
		return;
	}
	bounds = lowMap.equal_range(event.type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		it->second->signalEvent(event);
	}
}

void EventDistributor::registerEventListener(
	int type, EventListener *listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	map.insert(pair<int, EventListener*>(type, listener));
}

void EventDistributor::unregisterEventListener(
	int type, EventListener *listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		map.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
}

void EventDistributor::update(const SettingLeafNode *setting)
{
	assert(setting == &grabInput);
	SDL_WM_GrabInput(grabInput.getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}


// class QuitCommand

string EventDistributor::QuitCommand::execute(const vector<string> &tokens)
	throw()
{
	EventDistributor::instance()->quit();
	return "";
}

string EventDistributor::QuitCommand::help(const vector<string> &tokens) const
	throw()
{
	return "Use this command to stop the emulator\n";
}


// class AfterCommand

string EventDistributor::AfterCommand::execute(const vector<string>& tokens)
	throw(CommandException)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "time") {
		return afterTime(tokens);
	} else if (tokens[1] == "idle") {
		return afterIdle(tokens);
	} else if (tokens[1] == "info") {
		return afterInfo(tokens);
	} else if (tokens[1] == "cancel") {
		return afterCancel(tokens);
	}
	throw CommandException("Syntax error");
}

string EventDistributor::AfterCommand::afterTime(const vector<string>& tokens)
{
	return EventDistributor::instance()->afterNew(tokens, TIME);
}

string EventDistributor::AfterCommand::afterIdle(const vector<string>& tokens)
{
	return EventDistributor::instance()->afterNew(tokens, IDLE);
}

string EventDistributor::afterNew(const vector<string>& tokens,
                                  AfterType type)
{
	if (tokens.size() < 4) {
		throw CommandException("Syntax Error");
	}
	AfterCmd* cmd = new AfterCmd();
	cmd->type = type;
	float time = strtof(tokens[2].c_str(), NULL);
	if (time <= 0) {
		throw CommandException("Not a valid time specification");
	}
	cmd->time = time;
	string command = tokens[3];
	for (unsigned i = 4; i < tokens.size(); ++i) {
		command += ' ' + tokens[i];
	}
	cmd->command = command;
	cmd->id = ++lastAfterId;
	afterCmds[cmd->id] = cmd;
	
	EmuTime t = MSXCPU::instance()->getCurrentTime() + EmuDuration(time);
	Scheduler::instance()->setSyncPoint(t, cmd);

	ostringstream str;
	str << cmd->id << '\n';
	return str.str();
}

string EventDistributor::AfterCommand::afterInfo(const vector<string>& tokens)
{
	string result;
	EventDistributor* ed = EventDistributor::instance();
	for (map<unsigned, AfterCmd*>::const_iterator it = ed->afterCmds.begin();
	     it != ed->afterCmds.end(); ++it) {
		AfterCmd* cmd = it->second;
		ostringstream str;
		str << cmd->id << ": ";
		str << ((cmd->type == TIME) ? "time" : "idle") << ' ';
		str.precision(3);
		str << std::fixed << std::showpoint << cmd->time << ' ';
		str << cmd->command;
		result += str.str() + '\n';
	}
	return result;
}

string EventDistributor::AfterCommand::afterCancel(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw CommandException("Syntax error");
	}
	unsigned id = strtoul(tokens[2].c_str(), NULL, 10);
	EventDistributor* ed = EventDistributor::instance();
	map<unsigned, AfterCmd*>::iterator it = ed->afterCmds.find(id);
	if (it == ed->afterCmds.end()) {
		throw CommandException("No delayed command with this id");
	}
	Scheduler::instance()->removeSyncPoint(it->second);
	delete it->second;
	ed->afterCmds.erase(it);
	return "";
}

string EventDistributor::AfterCommand::help(const vector<string>& tokens) const
	throw()
{
	return "after time <seconds> <command>  execute a command after some time\n"
	       "after idle <seconds> <command>  execute a command after some time being idle\n"
	       "after info                      list all postponed commands\n"
	       "after cancel <id>               cancel the postponed command with given id\n";
}

void EventDistributor::AfterCommand::tabCompletion(const vector<string>& tokens)
	const throw()
{
	// TODO
}

void EventDistributor::AfterCmd::executeUntil(const EmuTime& time, int userData)
	throw()
{
	try {
		CommandController::instance()->executeCommand(command);
	} catch (CommandException& e) {
		CliCommOutput::instance().printWarning(
			"Error executig delayed command: " + e.getMessage());
	}
	EventDistributor* ed = EventDistributor::instance();
	ed->afterCmds.erase(id);
	delete this;
}

const string& EventDistributor::AfterCmd::schedName() const
{
	static const string sched_name("AfterCmd");
	return sched_name;
}

} // namespace openmsx
