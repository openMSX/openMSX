#include "AfterCommand.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "Schedulable.hh"
#include "EventDistributor.hh"
#include "InputEventFactory.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "RTSchedulable.hh"
#include "EmuTime.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "memory.hh"
#include "stl.hh"
#include "unreachable.hh"
#include <algorithm>
#include <iterator>
#include <sstream>

using std::ostringstream;
using std::string;
using std::vector;
using std::unique_ptr;
using std::move;

namespace openmsx {

class AfterCmd
{
public:
	virtual ~AfterCmd() {}
	string_ref getCommand() const;
	const string& getId() const;
	virtual string getType() const = 0;
	void execute();
protected:
	AfterCmd(AfterCommand& afterCommand,
		 const TclObject& command);
	unique_ptr<AfterCmd> removeSelf();

	AfterCommand& afterCommand;
	TclObject command;
	string id;
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
		      const TclObject& command, double time);
private:
	void executeUntil(EmuTime::param time) override;
	void schedulerDeleted() override;

	double time; // Zero when expired, otherwise the original duration (to
	             // be able to reschedule for 'after idle').
};

class AfterTimeCmd final : public AfterTimedCmd
{
public:
	AfterTimeCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     const TclObject& command, double time);
	string getType() const override;
};

class AfterIdleCmd final : public AfterTimedCmd
{
public:
	AfterIdleCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     const TclObject& command, double time);
	string getType() const override;
};

template<EventType T>
class AfterEventCmd final : public AfterCmd
{
public:
	AfterEventCmd(AfterCommand& afterCommand,
		      const TclObject& type,
		      const TclObject& command);
	string getType() const override;
private:
	const string type;
};

class AfterInputEventCmd final : public AfterCmd
{
public:
	AfterInputEventCmd(AfterCommand& afterCommand,
	                   AfterCommand::EventPtr event,
	                   const TclObject& command);
	string getType() const override;
	AfterCommand::EventPtr getEvent() const { return event; }
private:
	AfterCommand::EventPtr event;
};

class AfterRealTimeCmd final : public AfterCmd, private RTSchedulable
{
public:
	AfterRealTimeCmd(RTScheduler& rtScheduler, AfterCommand& afterCommand,
	                 const TclObject& command, double time);
	string getType() const override;

private:
	void executeRT() override;
};


AfterCommand::AfterCommand(Reactor& reactor_,
                           EventDistributor& eventDistributor_,
                           CommandController& commandController_)
	: Command(commandController_, "after")
	, reactor(reactor_)
	, eventDistributor(eventDistributor_)
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
		OPENMSX_JOY_HAT_EVENT, *this);
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
		OPENMSX_AFTER_TIMED_EVENT, *this);
}

AfterCommand::~AfterCommand()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_AFTER_TIMED_EVENT, *this);
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
		OPENMSX_JOY_HAT_EVENT, *this);
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

void AfterCommand::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	string_ref subCmd = tokens[1].getString();
	if (subCmd == "time") {
		afterTime(tokens, result);
	} else if (subCmd == "realtime") {
		afterRealTime(tokens, result);
	} else if (subCmd == "idle") {
		afterIdle(tokens, result);
	} else if (subCmd == "frame") {
		afterEvent<OPENMSX_FINISH_FRAME_EVENT>(tokens, result);
	} else if (subCmd == "break") {
		afterEvent<OPENMSX_BREAK_EVENT>(tokens, result);
	} else if (subCmd == "quit") {
		afterEvent<OPENMSX_QUIT_EVENT>(tokens, result);
	} else if (subCmd == "boot") {
		afterEvent<OPENMSX_BOOT_EVENT>(tokens, result);
	} else if (subCmd == "machine_switch") {
		afterEvent<OPENMSX_MACHINE_LOADED_EVENT>(tokens, result);
	} else if (subCmd == "info") {
		afterInfo(tokens, result);
	} else if (subCmd == "cancel") {
		afterCancel(tokens, result);
	} else {
		try {
			// A valid integer?
			int time = tokens[1].getInt(getInterpreter());
			afterTclTime(time, tokens, result);
		} catch (CommandException&) {
			try {
				// A valid event name?
				afterInputEvent(
					InputEventFactory::createInputEvent(
						tokens[1], getInterpreter()),
					tokens, result);
			} catch (MSXException&) {
				throw SyntaxError();
			}
		}
	}
}

static double getTime(Interpreter& interp, const TclObject& obj)
{
	double time = obj.getDouble(interp);
	if (time < 0) {
		throw CommandException("Not a valid time specification");
	}
	return time;
}

void AfterCommand::afterTime(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return;
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = make_unique<AfterTimeCmd>(
		motherBoard->getScheduler(), *this, tokens[3], time);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterRealTime(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = make_unique<AfterRealTimeCmd>(
		reactor.getRTScheduler(), *this, tokens[3], time);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterTclTime(
	int ms, array_ref<TclObject> tokens, TclObject& result)
{
	TclObject command;
	command.addListElements(std::begin(tokens) + 2, std::end(tokens));
	auto cmd = make_unique<AfterRealTimeCmd>(
		reactor.getRTScheduler(), *this, command, ms / 1000.0);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

template<EventType T>
void AfterCommand::afterEvent(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto cmd = make_unique<AfterEventCmd<T>>(
		*this, tokens[1], tokens[2]);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterInputEvent(
	const EventPtr& event, array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto cmd = make_unique<AfterInputEventCmd>(
		*this, event, tokens[2]);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterIdle(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return;
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = make_unique<AfterIdleCmd>(
		motherBoard->getScheduler(), *this, tokens[3], time);
	result.setString(cmd->getId());
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterInfo(array_ref<TclObject> /*tokens*/, TclObject& result)
{
	ostringstream str;
	for (auto& cmd : afterCmds) {
		str << cmd->getId() << ": ";
		str << cmd->getType() << ' ';
		if (auto cmd2 = dynamic_cast<const AfterTimedCmd*>(cmd.get())) {
			str.precision(3);
			str << std::fixed << std::showpoint << cmd2->getTime() << ' ';
		}
		str << cmd->getCommand()
		    << '\n';
	}
	result.setString(str.str());
}

void AfterCommand::afterCancel(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() < 3) {
		throw SyntaxError();
	}
	if (tokens.size() == 3) {
		auto id = tokens[2].getString();
		auto it = find_if(begin(afterCmds), end(afterCmds),
			[&](std::unique_ptr<AfterCmd>& e) { return e->getId() == id; });
		if (it != end(afterCmds)) {
			afterCmds.erase(it);
			return;
		}
	}
	TclObject command;
	command.addListElements(std::begin(tokens) + 2, std::end(tokens));
	string_ref cmdStr = command.getString();
	auto it = find_if(begin(afterCmds), end(afterCmds),
		[&](std::unique_ptr<AfterCmd>& e) { return e->getCommand() == cmdStr; });
	if (it != end(afterCmds)) {
		afterCmds.erase(it);
		// Tcl manual is not clear about this, but it seems
		// there's only occurence of this command canceled.
		// It's also not clear which of the (possibly) several
		// matches is canceled.
		return;
	}
	// It's not an error if no match is found
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
	if (tokens.size() == 2) {
		static const char* const cmds[] = {
			"time", "realtime", "idle", "frame", "break", "boot",
			"machine_switch", "info", "cancel",
		};
		completeString(tokens, cmds);
	}
	// TODO : make more complete
}

// Execute the cmds for which the predicate returns true, and erase those from afterCmds.
template<typename PRED> void AfterCommand::executeMatches(PRED pred)
{
	AfterCmds matches;
	// Usually there are very few matches (typically even 0 or 1), so no
	// need to reserve() space.
	auto p = partition_copy_remove(begin(afterCmds), end(afterCmds),
	                               std::back_inserter(matches), pred);
	afterCmds.erase(p.second, end(afterCmds));
	for (auto& c : matches) {
		c->execute();
	}
}

template<EventType T> struct AfterEventPred {
	bool operator()(const unique_ptr<AfterCmd>& x) const {
		return dynamic_cast<AfterEventCmd<T>*>(x.get()) != nullptr;
	}
};
template<EventType T> void AfterCommand::executeEvents()
{
	executeMatches(AfterEventPred<T>());
}

struct AfterEmuTimePred {
	bool operator()(const unique_ptr<AfterCmd>& x) const {
		if (auto* cmd = dynamic_cast<AfterTimedCmd*>(x.get())) {
			if (cmd->getTime() == 0.0) {
				return true;
			}
		}
		return false;
	}
};

struct AfterInputEventPred {
	AfterInputEventPred(AfterCommand::EventPtr event_)
		: event(std::move(event_)) {}
	bool operator()(const unique_ptr<AfterCmd>& x) const {
		if (auto* cmd = dynamic_cast<AfterInputEventCmd*>(x.get())) {
			if (cmd->getEvent()->matches(*event)) return true;
		}
		return false;
	}
	AfterCommand::EventPtr event;
};

int AfterCommand::signalEvent(const std::shared_ptr<const Event>& event)
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
		executeEvents<OPENMSX_MACHINE_LOADED_EVENT>();
	} else if (event->getType() == OPENMSX_AFTER_TIMED_EVENT) {
		executeMatches(AfterEmuTimePred());
	} else {
		executeMatches(AfterInputEventPred(event));
		for (auto& c : afterCmds) {
			if (auto* cmd = dynamic_cast<AfterIdleCmd*>(c.get())) {
				cmd->reschedule();
			}
		}
	}
	return 0;
}


// class AfterCmd

unsigned AfterCmd::lastAfterId = 0;

AfterCmd::AfterCmd(AfterCommand& afterCommand_, const TclObject& command_)
	: afterCommand(afterCommand_), command(command_)
{
	ostringstream str;
	str << "after#" << ++lastAfterId;
	id = str.str();
}

string_ref AfterCmd::getCommand() const
{
	return command.getString();
}

const string& AfterCmd::getId() const
{
	return id;
}

void AfterCmd::execute()
{
	try {
		command.executeCommand(afterCommand.getInterpreter());
	} catch (CommandException& e) {
		afterCommand.getCommandController().getCliComm().printWarning(
			"Error executing delayed command: " + e.getMessage());
	}
}

unique_ptr<AfterCmd> AfterCmd::removeSelf()
{
	auto it = rfind_if_unguarded(afterCommand.afterCmds,
		[&](std::unique_ptr<AfterCmd>& e) { return e.get() == this; });
	auto result = move(*it);
	afterCommand.afterCmds.erase(it);
	return result;
}


// class  AfterTimedCmd

AfterTimedCmd::AfterTimedCmd(
		Scheduler& scheduler_,
		AfterCommand& afterCommand_,
		const TclObject& command_, double time_)
	: AfterCmd(afterCommand_, command_)
	, Schedulable(scheduler_)
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

void AfterTimedCmd::executeUntil(EmuTime::param /*time*/)
{
	time = 0.0; // execute on next event
	afterCommand.eventDistributor.distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_AFTER_TIMED_EVENT));
}

void AfterTimedCmd::schedulerDeleted()
{
	removeSelf();
}


// class AfterTimeCmd

AfterTimeCmd::AfterTimeCmd(
		Scheduler& scheduler_,
		AfterCommand& afterCommand_,
		const TclObject& command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, command_, time_)
{
}

string AfterTimeCmd::getType() const
{
	return "time";
}


// class AfterIdleCmd

AfterIdleCmd::AfterIdleCmd(
		Scheduler& scheduler_,
		AfterCommand& afterCommand_,
		const TclObject& command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, command_, time_)
{
}

string AfterIdleCmd::getType() const
{
	return "idle";
}


// class AfterEventCmd

template<EventType T>
AfterEventCmd<T>::AfterEventCmd(
		AfterCommand& afterCommand_, const TclObject& type_,
		const TclObject& command_)
	: AfterCmd(afterCommand_, command_), type(type_.getString().str())
{
}

template<EventType T>
string AfterEventCmd<T>::getType() const
{
	return type;
}


// AfterInputEventCmd

AfterInputEventCmd::AfterInputEventCmd(
		AfterCommand& afterCommand_,
		AfterCommand::EventPtr event_,
		const TclObject& command_)
	: AfterCmd(afterCommand_, command_)
	, event(std::move(event_))
{
}

string AfterInputEventCmd::getType() const
{
	return event->toString();
}

// class AfterRealTimeCmd

AfterRealTimeCmd::AfterRealTimeCmd(
		RTScheduler& rtScheduler, AfterCommand& afterCommand_,
		const TclObject& command_, double time)
	: AfterCmd(afterCommand_, command_)
	, RTSchedulable(rtScheduler)
{
	scheduleRT(uint64_t(time * 1e6)); // micro seconds
}

string AfterRealTimeCmd::getType() const
{
	return "realtime";
}

void AfterRealTimeCmd::executeRT()
{
	// Remove self before executing, but keep self alive till the end of
	// this method. Otherwise execute could execute 'after cancel ..' and
	// removeSelf() asserts that it can't find itself anymore.
	auto self = removeSelf();
	execute();
}

} // namespace openmsx
