// $Id$

#include <iostream>
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "openmsx.hh"
#include "config.h"
#include "Scheduler.hh"
#include "ConsoleManager.hh"
#include "CommandController.hh"
#include "HotKey.hh"


EventDistributor::EventDistributor()
{
	CommandController::instance()->registerCommand(bindCmd,   "bind");
	CommandController::instance()->registerCommand(unbindCmd, "unbind");
}

EventDistributor::~EventDistributor()
{
	CommandController::instance()->unregisterCommand("bind");
	CommandController::instance()->unregisterCommand("unbind");
}

EventDistributor *EventDistributor::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}
EventDistributor *EventDistributor::oneInstance = NULL;


/**
 * This is the main-loop. It waits for events and 
 *  queues them for later synchronous delivery
 * Note: this method runs in a different thread!!
 */
void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
	
		bool anySync = false;
		
		syncMutex.grab();
		std::multimap<int, EventListener*>::iterator it;
		for (it = syncMap.lower_bound(event.type);
		     (it != syncMap.end()) && (it->first == event.type);
		     it++) {
			queueMutex.grab();
			queue.push(std::pair<SDL_Event, EventListener*>(event, it->second));
			queueMutex.release();
			anySync = true;
		}
		syncMutex.release();
		
		if (anySync) {
			Scheduler::instance()->removeSyncPoint(this);
			Scheduler::instance()->setSyncPoint(Scheduler::ASAP, this);
		}
	}
	// this loop never ends
	assert(false);
}

void EventDistributor::executeUntilEmuTime(const EmuTime &time, int userdata)
{
	queueMutex.grab();
	while (!queue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = queue.front();
		queue.pop();
		queueMutex.release();	// temporary release queue mutex
		pair.second->signalEvent(pair.first);
		queueMutex.grab();	// retake queue mutex
	}
	queueMutex.release();
}


void EventDistributor::registerEventListener(int type, EventListener *listener)
{
	syncMutex.grab();
	syncMap.insert(std::pair<int, EventListener*>(type, listener));
	syncMutex.release();
}
void EventDistributor::unregisterEventListener(int type, EventListener *listener)
{
	syncMutex.grab();
	std::multimap<int, EventListener*>::iterator it;
	for (it = syncMap.lower_bound(type);
	     (it != syncMap.end()) && (it->first == type);
	     it++) {
		if (it->second == listener) {
			syncMap.erase(it);
			break;
		}
	}
	syncMutex.release();
}


void EventDistributor::BindCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 1:
		// TODO show all bounded keys
		break;
	case 2:
		// TODO show binding for this key
		break;
	case 3: {
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		HotKey::instance()->registerHotKeyCommand(key, tokens[2]);
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void EventDistributor::BindCmd::help(const std::vector<std::string> &tokens)
{
	// TODO
	ConsoleManager::instance()->print("bind:  TODO");
}

void EventDistributor::UnbindCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 2:
		// TODO unbind all for this key
		break;
	case 3: {
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		HotKey::instance()->unregisterHotKeyCommand(key, tokens[2]);
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void EventDistributor::UnbindCmd::help(const std::vector<std::string> &tokens)
{
	// TODO
	ConsoleManager::instance()->print("unbind:  TODO");
}
