// $Id$

#include "AfterCommand.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "Schedulable.hh"
#include "EventDistributor.hh"
#include "MSXEventDistributor.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "Alarm.hh"
#include "EmuTime.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include "unreachable.hh"
#include <algorithm>
#include <cstdlib>
#include <sstream>

using std::ostringstream;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

class AfterCmd
{
public:
	virtual ~AfterCmd();
	const std::string& getCommand() const;
	const std::string& getId() const;
	virtual const std::string& getType() const = 0;
	void execute();
protected:
	AfterCmd(AfterCommand& afterCommand,
		 const std::string& command);
	shared_ptr<AfterCmd> removeSelf();
private:
	AfterCommand& afterCommand;
	std::string command;
	std::string id;
	static unsigned lastAfterId;
};

class AfterTimedCmd : public AfterCmd, private Schedulable
{
public:
	double getTime() const;
	void reschedule();
protected:
	AfterTimedCmd(Scheduler& scheduler,
		      AfterCommand& afterCommand,
		      const std::string& command, double time);
private:
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual void schedulerDeleted();
	virtual const std::string& schedName() const;

	double time;
};

class AfterTimeCmd : public AfterTimedCmd
{
public:
	AfterTimeCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     const std::string& command, double time);
	virtual const std::string& getType() const;
};

class AfterIdleCmd : public AfterTimedCmd
{
public:
	AfterIdleCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     const std::string& command, double time);
	virtual const std::string& getType() const;
};

template<EventType T>
class AfterEventCmd : public AfterCmd
{
public:
	AfterEventCmd(AfterCommand& afterCommand,
		      const std::string& type,
		      const std::string& command);
	virtual const std::string& getType() const;
private:
	const std::string type;
};

class AfterRealTimeCmd : public AfterCmd, private Alarm
{
public:
	AfterRealTimeCmd(AfterCommand& afterCommand,
	                 EventDistributor& eventDistributor,
	                 const std::string& command, double time);
	virtual ~AfterRealTimeCmd();
	virtual const std::string& getType() const;
	bool hasExpired() const;

private:
	virtual bool alarm();

	EventDistributor& eventDistributor;
	bool expired;
};


AfterCommand::AfterCommand(Reactor& reactor_,
                           EventDistributor& eventDistributor_,
                           CommandController& commandController)
	: SimpleCommand(commandController, "after")
	, reactor(reactor_)
	, eventDistributor(eventDistributor_)
	, msxEvents(NULL)
{
	// TODO DETACHED <-> EMU types should be cleaned up
	//      (moved to event iso listener?)
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_FINISH_FRAME_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_BREAK_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_QUIT_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_BOOT_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MACHINE_LOADED_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_AFTER_REALTIME_EVENT, *this);

	machineSwitch();
}

AfterCommand::~AfterCommand()
{
	if (msxEvents) {
		msxEvents->unregisterEventListener(*this);
	}

	eventDistributor.unregisterEventListener(
		OPENMSX_AFTER_REALTIME_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MACHINE_LOADED_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_BOOT_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_QUIT_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_BREAK_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_FINISH_FRAME_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_KEY_UP_EVENT, *this);
}

string AfterCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "time") {
		return afterTime(tokens);
	} else if (tokens[1] == "realtime") {
		return afterRealTime(tokens);
	} else if (tokens[1] == "idle") {
		return afterIdle(tokens);
	} else if (tokens[1] == "frame") {
		return afterEvent<OPENMSX_FINISH_FRAME_EVENT>(tokens);
	} else if (tokens[1] == "break") {
		return afterEvent<OPENMSX_BREAK_EVENT>(tokens);
	} else if (tokens[1] == "quit") {
		return afterEvent<OPENMSX_QUIT_EVENT>(tokens);
	} else if (tokens[1] == "boot") {
		return afterEvent<OPENMSX_BOOT_EVENT>(tokens);
	} else if (tokens[1] == "machine_switch") {
		return afterEvent<OPENMSX_MACHINE_LOADED_EVENT>(tokens);
	} else if (tokens[1] == "info") {
		return afterInfo(tokens);
	} else if (tokens[1] == "cancel") {
		return afterCancel(tokens);
	}
	throw SyntaxError();
}

static double getTime(const string& str)
{
	double time = StringOp::stringToDouble(str);
	if (time < 0) {
		throw CommandException("Not a valid time specification");
	}
	return time;
}

string AfterCommand::afterTime(const vector<string>& tokens)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		return "";
	}
	double time = getTime(tokens[2]);
	shared_ptr<AfterCmd> cmd(new AfterTimeCmd(
		motherBoard->getScheduler(), *this, tokens[3], time));
	afterCmds.push_back(cmd);
	return cmd->getId();
}

string AfterCommand::afterRealTime(const vector<string>& tokens)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	double time = getTime(tokens[2]);
	shared_ptr<AfterCmd> cmd(new AfterRealTimeCmd(
		*this, eventDistributor, tokens[3], time));
	afterCmds.push_back(cmd);
	return cmd->getId();
}

template<EventType T>
string AfterCommand::afterEvent(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	shared_ptr<AfterCmd> cmd(new AfterEventCmd<T>(*this, tokens[1], tokens[2]));
	afterCmds.push_back(cmd);
	return cmd->getId();
}

string AfterCommand::afterIdle(const vector<string>& tokens)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		return "";
	}
	double time = getTime(tokens[2]);
	shared_ptr<AfterCmd> cmd(new AfterIdleCmd(
		motherBoard->getScheduler(), *this, tokens[3], time));
	afterCmds.push_back(cmd);
	return cmd->getId();
}

string AfterCommand::afterInfo(const vector<string>& /*tokens*/)
{
	string result;
	for (AfterCmds::const_iterator it = afterCmds.begin();
	     it != afterCmds.end(); ++it) {
		const AfterCmd* cmd = it->get();
		ostringstream str;
		str << cmd->getId() << ": ";
		str << cmd->getType() << ' ';
		if (dynamic_cast<const AfterTimedCmd*>(cmd)) {
			const AfterTimedCmd* cmd2 = static_cast<const AfterTimedCmd*>(cmd);
			str.precision(3);
			str << std::fixed << std::showpoint << cmd2->getTime() << ' ';
		}
		str << cmd->getCommand();
		result += str.str() + '\n';
	}
	return result;
}

string AfterCommand::afterCancel(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	AfterCmds::iterator it = afterCmds.begin();
	for (/**/; it != afterCmds.end(); ++it) {
		if ((*it)->getId() == tokens[2]) {
			break;
		}
	}
	if (it == afterCmds.end()) {
		throw CommandException("No delayed command with this id");
	}
	afterCmds.erase(it);
	return "";
}

string AfterCommand::help(const vector<string>& /*tokens*/) const
{
	return "after time     <seconds> <command>  execute a command after some time (MSX time)\n"
	       "after realtime <seconds> <command>  execute a command after some time (realtime)\n"
	       "after idle     <seconds> <command>  execute a command after some time being idle\n"
	       "after frame <command>               execute a command after a new frame is drawn\n"
	       "after break <command>               execute a command after a breakpoint is reached\n"
	       "after boot <command>                execute a command after a (re)boot\n"
	       "after machine_switch <command>      execute a command after a switch to a new machine\n"
	       "after info                          list all postponed commands\n"
	       "after cancel <id>                   cancel the postponed command with given id\n";
}

void AfterCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size()==2) {
		set<string> cmds;
		cmds.insert("time");
		cmds.insert("realtime");
		cmds.insert("idle");
		cmds.insert("frame");
		cmds.insert("break");
		cmds.insert("boot");
		cmds.insert("machine_switch");
		cmds.insert("info");
		cmds.insert("cancel");
		completeString(tokens, cmds);
	}
	// TODO : make more complete
}


template<EventType T> struct AfterEventPred {
	bool operator()(shared_ptr<AfterCmd> x) const {
		return !dynamic_cast<AfterEventCmd<T>*>(x.get());
	}
};
template<EventType T> void AfterCommand::executeEvents()
{
	AfterCmds::iterator it = partition(afterCmds.begin(), afterCmds.end(),
	                                   AfterEventPred<T>());
	AfterCmds tmp(it, afterCmds.end());
	afterCmds.erase(it, afterCmds.end());

	for (AfterCmds::iterator it = tmp.begin(); it != tmp.end(); ++it) {
		(*it)->execute();
	}
}

struct AfterTimePred {
	bool operator()(shared_ptr<AfterCmd> x) const {
		if (AfterRealTimeCmd* realtimeCmd =
		              dynamic_cast<AfterRealTimeCmd*>(x.get())) {
			if (realtimeCmd->hasExpired()) {
				return false;
			}
		}
		return true;
	}
};
void AfterCommand::executeRealTime()
{
	AfterCmds::iterator it = partition(afterCmds.begin(), afterCmds.end(),
	                                   AfterTimePred());
	AfterCmds tmp(it, afterCmds.end());
	afterCmds.erase(it, afterCmds.end());

	for (AfterCmds::iterator it = tmp.begin(); it != tmp.end(); ++it) {
		(*it)->execute();
	}
}

bool AfterCommand::signalEvent(shared_ptr<const Event> event)
{
	if (event->getType() == OPENMSX_FINISH_FRAME_EVENT) {
		executeEvents<OPENMSX_FINISH_FRAME_EVENT>();
	} else if (event->getType() == OPENMSX_BREAK_EVENT) {
		executeEvents<OPENMSX_BREAK_EVENT>();
	} else if (event->getType() == OPENMSX_BOOT_EVENT) {
		executeEvents<OPENMSX_BOOT_EVENT>();
	} else if (event->getType() == OPENMSX_QUIT_EVENT) {
		executeEvents<OPENMSX_QUIT_EVENT>();
	} else if (event->getType() == OPENMSX_MACHINE_LOADED_EVENT) {
		machineSwitch();
		executeEvents<OPENMSX_MACHINE_LOADED_EVENT>();
	} else if (event->getType() == OPENMSX_AFTER_REALTIME_EVENT) {
		executeRealTime();
	} else {
		for (AfterCmds::const_iterator it = afterCmds.begin();
		     it != afterCmds.end(); ++it) {
			if (AfterIdleCmd* idleCmd =
			              dynamic_cast<AfterIdleCmd*>(it->get())) {
				idleCmd->reschedule();
			}
		}
	}
	return true;
}

void AfterCommand::machineSwitch()
{
	if (msxEvents) {
		msxEvents->unregisterEventListener(*this);
	}
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	msxEvents = motherBoard ? &motherBoard->getMSXEventDistributor() : NULL;
	if (msxEvents) {
		msxEvents->registerEventListener(*this);
	}
}

void AfterCommand::signalEvent(shared_ptr<const Event> event,
                               EmuTime::param time)
{
	// TODO
}


// class AfterCmd

unsigned AfterCmd::lastAfterId = 0;

AfterCmd::AfterCmd(AfterCommand& afterCommand_, const string& command_)
	: afterCommand(afterCommand_), command(command_)
{
	ostringstream str;
	str << "after#" << ++lastAfterId;
	id = str.str();
}

AfterCmd::~AfterCmd()
{
}

const string& AfterCmd::getCommand() const
{
	return command;
}

const string& AfterCmd::getId() const
{
	return id;
}

void AfterCmd::execute()
{
	try {
		afterCommand.getCommandController().executeCommand(command);
	} catch (CommandException& e) {
		afterCommand.getCommandController().getCliComm().printWarning(
			"Error executing delayed command: " + e.getMessage());
	}
}

shared_ptr<AfterCmd> AfterCmd::removeSelf()
{
	for (AfterCommand::AfterCmds::iterator it = afterCommand.afterCmds.begin();
	     it != afterCommand.afterCmds.end(); ++it) {
		if (it->get() == this) {
			shared_ptr<AfterCmd> result = *it;
			afterCommand.afterCmds.erase(it);
			return result;
		}
	}
	UNREACHABLE; return shared_ptr<AfterCmd>(0);
}


// class  AfterTimedCmd

AfterTimedCmd::AfterTimedCmd(
		Scheduler& scheduler,
		AfterCommand& afterCommand,
		const string& command, double time_)
	: AfterCmd(afterCommand, command)
	, Schedulable(scheduler)
	, time(time_)

{
	reschedule();
}

double AfterTimedCmd::getTime() const
{
	return time;
}

void AfterTimedCmd::reschedule()
{
	removeSyncPoint();
	setSyncPoint(getCurrentTime() + EmuDuration(time));
}

void AfterTimedCmd::executeUntil(EmuTime::param /*time*/,
                                 int /*userData*/)
{
	shared_ptr<AfterCmd> self = removeSelf();
	execute();
}

void AfterTimedCmd::schedulerDeleted()
{
	removeSelf();
}

const string& AfterTimedCmd::schedName() const
{
	static const string sched_name("AfterCmd");
	return sched_name;
}


// class AfterTimeCmd

AfterTimeCmd::AfterTimeCmd(
		Scheduler& scheduler,
		AfterCommand& afterCommand,
		const string& command, double time)
	: AfterTimedCmd(scheduler, afterCommand, command, time)
{
}

const string& AfterTimeCmd::getType() const
{
	static const string type("time");
	return type;
}


// class AfterIdleCmd

AfterIdleCmd::AfterIdleCmd(
		Scheduler& scheduler,
		AfterCommand& afterCommand,
		const string& command, double time)
	: AfterTimedCmd(scheduler, afterCommand, command, time)
{
}

const string& AfterIdleCmd::getType() const
{
	static const string type("idle");
	return type;
}


// class AfterEventCmd

template<EventType T>
AfterEventCmd<T>::AfterEventCmd(
		AfterCommand& afterCommand, const string& type_,
		const string& command)
	: AfterCmd(afterCommand, command), type(type_)
{
}

template<EventType T>
const string& AfterEventCmd<T>::getType() const
{
	return type;
}


// class AfterRealTimeCmd

AfterRealTimeCmd::AfterRealTimeCmd(
		AfterCommand& afterCommand, EventDistributor& eventDistributor_,
		const std::string& command, double time)
	: AfterCmd(afterCommand, command)
	, eventDistributor(eventDistributor_)
	, expired(false)
{
	schedule(unsigned(time * 1000000)); // micro seconds
}

AfterRealTimeCmd::~AfterRealTimeCmd()
{
	prepareDelete();
}

const std::string& AfterRealTimeCmd::getType() const
{
	static const string type("realtime");
	return type;
}

bool AfterRealTimeCmd::alarm()
{
	// this runs in a different thread, so we can't directly execute the
	// command here
	expired = true;
	eventDistributor.distributeEvent(
			new SimpleEvent(OPENMSX_AFTER_REALTIME_EVENT));
	return false; // don't repeat alarm
}

bool AfterRealTimeCmd::hasExpired() const
{
	return expired;
}

} // namespace openmsx
