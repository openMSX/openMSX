// $Id$

#include <sstream>
#include "AfterCommand.hh"
#include "CommandController.hh"
#include "CliCommOutput.hh"
#include "Scheduler.hh"
#include "EventDistributor.hh"

using std::ostringstream;

namespace openmsx {

AfterCommand::AfterCommand()
	: lastAfterId(0)
{
	EventDistributor::instance().registerEventListener(SDL_KEYUP, this);
	EventDistributor::instance().registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEMOTION, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONUP, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_JOYAXISMOTION, this);
	EventDistributor::instance().registerEventListener(SDL_JOYBUTTONUP, this);
	EventDistributor::instance().registerEventListener(SDL_JOYBUTTONDOWN, this);
	CommandController::instance().registerCommand(this, "after");
}

AfterCommand::~AfterCommand()
{
	CommandController::instance().unregisterCommand(this, "after");
	EventDistributor::instance().registerEventListener(SDL_JOYBUTTONDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_JOYBUTTONUP, this);
	EventDistributor::instance().registerEventListener(SDL_JOYAXISMOTION, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEBUTTONUP, this);
	EventDistributor::instance().registerEventListener(SDL_MOUSEMOTION, this);
	EventDistributor::instance().registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance().registerEventListener(SDL_KEYUP, this);
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
	} else if (tokens[1] == "info") {
		return afterInfo(tokens);
	} else if (tokens[1] == "cancel") {
		return afterCancel(tokens);
	}
	throw SyntaxError();
}

string AfterCommand::afterTime(const vector<string>& tokens)
{
	return afterNew(tokens, TIME);
}

string AfterCommand::afterIdle(const vector<string>& tokens)
{
	return afterNew(tokens, IDLE);
}

string AfterCommand::afterNew(const vector<string>& tokens, AfterType type)
{
	if (tokens.size() < 4) {
		throw SyntaxError();
	}
	AfterCmd* cmd = new AfterCmd(*this);
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
	
	EmuTime t = Scheduler::instance().getCurrentTime() + EmuDuration(time);
	Scheduler::instance().setSyncPoint(t, cmd);

	ostringstream str;
	str << cmd->id << '\n';
	return str.str();
}

string AfterCommand::afterInfo(const vector<string>& tokens)
{
	string result;
	for (map<unsigned, AfterCmd*>::const_iterator it = afterCmds.begin();
	     it != afterCmds.end(); ++it) {
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

string AfterCommand::afterCancel(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned id = strtoul(tokens[2].c_str(), NULL, 10);
	map<unsigned, AfterCmd*>::iterator it = afterCmds.find(id);
	if (it == afterCmds.end()) {
		throw CommandException("No delayed command with this id");
	}
	Scheduler::instance().removeSyncPoint(it->second);
	delete it->second;
	afterCmds.erase(it);
	return "";
}

string AfterCommand::help(const vector<string>& tokens) const
	throw()
{
	return "after time <seconds> <command>  execute a command after some time\n"
	       "after idle <seconds> <command>  execute a command after some time being idle\n"
	       "after info                      list all postponed commands\n"
	       "after cancel <id>               cancel the postponed command with given id\n";
}

void AfterCommand::tabCompletion(const vector<string>& tokens)
	const throw()
{
	// TODO
}

bool AfterCommand::signalEvent(const SDL_Event& event) throw()
{
	for (map<unsigned, AfterCmd*>::const_iterator it = afterCmds.begin();
	     it != afterCmds.end(); ++it) {
		AfterCmd* cmd = it->second;
		if (cmd->type == IDLE) {
			Scheduler::instance().removeSyncPoint(cmd);
			EmuTime t = Scheduler::instance().getCurrentTime() +
			            EmuDuration(cmd->time);
			Scheduler::instance().setSyncPoint(t, cmd);
		}
	}
	return true;
}


// class AfterCmd

AfterCommand::AfterCmd::AfterCmd(AfterCommand& parent_)
	: parent(parent_)
{
}

AfterCommand::AfterCmd::~AfterCmd()
{
}

void AfterCommand::AfterCmd::executeUntil(const EmuTime& time, int userData)
	throw()
{
	try {
		CommandController::instance().executeCommand(command);
	} catch (CommandException& e) {
		CliCommOutput::instance().printWarning(
			"Error executig delayed command: " + e.getMessage());
	}
	parent.afterCmds.erase(id);
	delete this;
}

const string& AfterCommand::AfterCmd::schedName() const
{
	static const string sched_name("AfterCmd");
	return sched_name;
}

} // namespace openmsx
