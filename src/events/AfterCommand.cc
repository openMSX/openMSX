// $Id$

#include "AfterCommand.hh"
#include "CommandController.hh"
#include "CliCommOutput.hh"
#include "Scheduler.hh"
#include "EventDistributor.hh"
#include <cstdlib>
#include <sstream>

using std::ostringstream;

namespace openmsx {

AfterCommand::AfterCmdMap AfterCommand::afterCmds;

AfterCommand::AfterCommand()
{
	EventDistributor::instance().registerEventListener(KEY_UP_EVENT, *this);
	EventDistributor::instance().registerEventListener(KEY_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(MOUSE_MOTION_EVENT, *this);
	EventDistributor::instance().registerEventListener(MOUSE_BUTTON_UP_EVENT, *this);
	EventDistributor::instance().registerEventListener(MOUSE_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(JOY_AXIS_MOTION_EVENT, *this);
	EventDistributor::instance().registerEventListener(JOY_BUTTON_UP_EVENT, *this);
	EventDistributor::instance().registerEventListener(JOY_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(FINISH_FRAME_EVENT, *this);
	CommandController::instance().registerCommand(this, "after");
}

AfterCommand::~AfterCommand()
{
	CommandController::instance().unregisterCommand(this, "after");
	EventDistributor::instance().unregisterEventListener(FINISH_FRAME_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(JOY_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(JOY_BUTTON_UP_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(JOY_AXIS_MOTION_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(MOUSE_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(MOUSE_BUTTON_UP_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(MOUSE_MOTION_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(KEY_DOWN_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(KEY_UP_EVENT, *this);
}

string AfterCommand::execute(const vector<string>& tokens)
	throw(CommandException)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "time") {
		return afterTime(tokens);
	} else if (tokens[1] == "idle") {
		return afterIdle(tokens);
	} else if (tokens[1] == "frame") {
		return afterFrame(tokens);
	} else if (tokens[1] == "info") {
		return afterInfo(tokens);
	} else if (tokens[1] == "cancel") {
		return afterCancel(tokens);
	}
	throw SyntaxError();
}

static float getTime(const string& str)
{
	// Use "strtod" instead of "strtof" because the latter doesn't exist in
	// the libc of FreeBSD. It's not equivalent, but we don't need maximum
	// precision or range checks, so it's close enough.
	float time = (float)strtod(str.c_str(), NULL);
	if (time <= 0) {
		throw CommandException("Not a valid time specification");
	}
	return time;
}

string AfterCommand::afterTime(const vector<string>& tokens)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	float time = getTime(tokens[2]);
	AfterTimeCmd* cmd = new AfterTimeCmd(tokens[3], time);
	return cmd->getId();
}

string AfterCommand::afterIdle(const vector<string>& tokens)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	float time = getTime(tokens[2]);
	AfterIdleCmd* cmd = new AfterIdleCmd(tokens[3], time);
	return cmd->getId();
}

string AfterCommand::afterFrame(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	AfterFrameCmd* cmd = new AfterFrameCmd(tokens[2]);
	return cmd->getId();
}

string AfterCommand::afterInfo(const vector<string>& tokens)
{
	string result;
	for (AfterCmdMap::const_iterator it = afterCmds.begin();
	     it != afterCmds.end(); ++it) {
		const AfterCmd* cmd = it->second;
		ostringstream str;
		str << cmd->getId() << ": ";
		str << cmd->getType() << ' ';
		if (dynamic_cast<const AfterTimeCmd*>(cmd)) {
			const AfterTimeCmd* cmd2 = static_cast<const AfterTimeCmd*>(cmd);
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
	AfterCmdMap::iterator it = afterCmds.find(tokens[2]);
	if (it == afterCmds.end()) {
		throw CommandException("No delayed command with this id");
	}
	delete it->second;
	return "";
}

string AfterCommand::help(const vector<string>& tokens) const
	throw()
{
	return "after time <seconds> <command>  execute a command after some time\n"
	       "after idle <seconds> <command>  execute a command after some time being idle\n"
	       "after frame <command>           execute a command after a new frame is drawn\n"
	       "after info                      list all postponed commands\n"
	       "after cancel <id>               cancel the postponed command with given id\n";
}

void AfterCommand::tabCompletion(vector<string>& tokens)
	const throw()
{
	// TODO
}

bool AfterCommand::signalEvent(const Event& event) throw()
{
	if (event.getType() == FINISH_FRAME_EVENT) {
		vector<AfterCmd*> tmp; // make copy because map will change
		for (AfterCmdMap::const_iterator it = afterCmds.begin();
		     it != afterCmds.end(); ++it) {
			if (dynamic_cast<AfterFrameCmd*>(it->second)) {
				tmp.push_back(it->second);
			}
		}
		for (vector<AfterCmd*>::iterator it = tmp.begin();
		     it != tmp.end(); ++it) {
			(*it)->execute();
		}
	} else {
		for (AfterCmdMap::const_iterator it = afterCmds.begin();
		     it != afterCmds.end(); ++it) {
			if (dynamic_cast<AfterTimeCmd*>(it->second)) {
				static_cast<AfterTimeCmd*>(it->second)->reschedule();
			}
		}
	}
	return true;
}


// class AfterCmd

unsigned AfterCommand::AfterCmd::lastAfterId = 0;

AfterCommand::AfterCmd::AfterCmd(const string& command_)
	: command(command_)
{
	ostringstream str;
	str << "after#" << ++lastAfterId;
	id = str.str();
	
	afterCmds[id] = this;
}

AfterCommand::AfterCmd::~AfterCmd()
{
	afterCmds.erase(id);
}

const string& AfterCommand::AfterCmd::getCommand() const
{
	return command;
}

const string& AfterCommand::AfterCmd::getId() const
{
	return id;
}

void AfterCommand::AfterCmd::execute() throw()
{
	try {
		CommandController::instance().executeCommand(command);
	} catch (CommandException& e) {
		CliCommOutput::instance().printWarning(
			"Error executig delayed command: " + e.getMessage());
	}
	delete this;
}


// class  AfterTimedCmd

AfterCommand::AfterTimedCmd::AfterTimedCmd(const string& command, float time_)
	: AfterCmd(command), time(time_)
{
	reschedule();
}

AfterCommand::AfterTimedCmd::~AfterTimedCmd()
{
	Scheduler::instance().removeSyncPoint(this);
}

float AfterCommand::AfterTimedCmd::getTime() const
{
	return time;
}

void AfterCommand::AfterTimedCmd::reschedule()
{
	Scheduler::instance().removeSyncPoint(this);
	EmuTime t = Scheduler::instance().getCurrentTime() + EmuDuration(time);
	Scheduler::instance().setSyncPoint(t, this);
}

void AfterCommand::AfterTimedCmd::executeUntil(const EmuTime& time, int userData)
	throw()
{
	execute();
}

const string& AfterCommand::AfterTimedCmd::schedName() const
{
	static const string sched_name("AfterCmd");
	return sched_name;
}


//class AfterFrameCmd

AfterCommand::AfterFrameCmd::AfterFrameCmd(const string& command)
	: AfterCmd(command)
{
}

const string& AfterCommand::AfterFrameCmd::getType() const
{
	static const string type("frame");
	return type;
}

// class AfterTimeCmd

AfterCommand::AfterTimeCmd::AfterTimeCmd(const string& command, float time)
	: AfterTimedCmd(command, time)
{
}

const string& AfterCommand::AfterTimeCmd::getType() const
{
	static const string type("time");
	return type;
}


// class AfterIdleCmd

AfterCommand::AfterIdleCmd::AfterIdleCmd(const string& command, float time)
	: AfterTimedCmd(command, time)
{
}

const string& AfterCommand::AfterIdleCmd::getType() const
{
	static const string type("idle");
	return type;
}

} // namespace openmsx
