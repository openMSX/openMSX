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
#include "ranges.hh"
#include "stl.hh"
#include "view.hh"
#include <iterator>
#include <memory>
#include <sstream>

using std::move;
using std::ostringstream;
using std::string;
using std::string_view;
using std::vector;
using std::unique_ptr;

namespace openmsx {

class AfterCmd
{
public:
	virtual ~AfterCmd() = default;
	[[nodiscard]] string_view getCommand() const;
	[[nodiscard]] const string& getId() const;
	[[nodiscard]] virtual string getType() const = 0;
	void execute();
protected:
	AfterCmd(AfterCommand& afterCommand,
		 TclObject command);
	unique_ptr<AfterCmd> removeSelf();

	AfterCommand& afterCommand;
	TclObject command;
	string id;
	static inline unsigned lastAfterId = 0;
};

class AfterTimedCmd : public AfterCmd, private Schedulable
{
public:
	[[nodiscard]] double getTime() const;
	void reschedule();
protected:
	AfterTimedCmd(Scheduler& scheduler,
		      AfterCommand& afterCommand,
		      TclObject command, double time);
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
		     TclObject command, double time);
	[[nodiscard]] string getType() const override;
};

class AfterIdleCmd final : public AfterTimedCmd
{
public:
	AfterIdleCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     TclObject command, double time);
	[[nodiscard]] string getType() const override;
};

template<EventType T>
class AfterEventCmd final : public AfterCmd
{
public:
	AfterEventCmd(AfterCommand& afterCommand,
		      const TclObject& type,
		      TclObject command);
	[[nodiscard]] string getType() const override;
private:
	const string type;
};

class AfterInputEventCmd final : public AfterCmd
{
public:
	AfterInputEventCmd(AfterCommand& afterCommand,
	                   AfterCommand::EventPtr event,
	                   TclObject command);
	[[nodiscard]] string getType() const override;
	[[nodiscard]] AfterCommand::EventPtr getEvent() const { return event; }
private:
	AfterCommand::EventPtr event;
};

class AfterRealTimeCmd final : public AfterCmd, private RTSchedulable
{
public:
	AfterRealTimeCmd(RTScheduler& rtScheduler, AfterCommand& afterCommand,
	                 TclObject command, double time);
	[[nodiscard]] string getType() const override;

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
		OPENMSX_MOUSE_WHEEL_EVENT, *this);
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
		OPENMSX_MOUSE_WHEEL_EVENT, *this);
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

void AfterCommand::execute(span<const TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	string_view subCmd = tokens[1].getString();
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
		// A valid integer?
		if (auto time = tokens[1].getOptionalInt()) {
			afterTclTime(*time, tokens, result);
		} else {
			// A valid event name?
			try {
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

void AfterCommand::afterTime(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "seconds command");
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return;
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = std::make_unique<AfterTimeCmd>(
		motherBoard->getScheduler(), *this, tokens[3], time);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterRealTime(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "seconds command");
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = std::make_unique<AfterRealTimeCmd>(
		reactor.getRTScheduler(), *this, tokens[3], time);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterTclTime(
	int ms, span<const TclObject> tokens, TclObject& result)
{
	TclObject command;
	command.addListElements(view::drop(tokens, 2));
	auto cmd = std::make_unique<AfterRealTimeCmd>(
		reactor.getRTScheduler(), *this, command, ms / 1000.0);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

template<EventType T>
void AfterCommand::afterEvent(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "command");
	auto cmd = std::make_unique<AfterEventCmd<T>>(*this, tokens[1], tokens[2]);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterInputEvent(
	const EventPtr& event, span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "command");
	auto cmd = std::make_unique<AfterInputEventCmd>(*this, event, tokens[2]);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterIdle(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "seconds command");
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return;
	double time = getTime(getInterpreter(), tokens[2]);
	auto cmd = std::make_unique<AfterIdleCmd>(
		motherBoard->getScheduler(), *this, tokens[3], time);
	result = cmd->getId();
	afterCmds.push_back(move(cmd));
}

void AfterCommand::afterInfo(span<const TclObject> /*tokens*/, TclObject& result)
{
	ostringstream str;
	for (auto& cmd : afterCmds) {
		str << cmd->getId() << ": ";
		str << cmd->getType() << ' ';
		if (const auto* cmd2 = dynamic_cast<const AfterTimedCmd*>(cmd.get())) {
			str.precision(3);
			str << std::fixed << std::showpoint << cmd2->getTime() << ' ';
		}
		str << cmd->getCommand()
		    << '\n';
	}
	result = str.str();
}

void AfterCommand::afterCancel(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, AtLeast{3}, "id|command");
	if (tokens.size() == 3) {
		auto id = tokens[2].getString();
		if (auto it = ranges::find_if(afterCmds,
		                              [&](auto& e) { return e->getId() == id; });
		    it != end(afterCmds)) {
			afterCmds.erase(it);
			return;
		}
	}
	TclObject command;
	command.addListElements(view::drop(tokens, 2));
	string_view cmdStr = command.getString();
	if (auto it = ranges::find_if(afterCmds,
	                              [&](auto& e) { return e->getCommand() == cmdStr; });
	    it != end(afterCmds)) {
		afterCmds.erase(it);
		// Tcl manual is not clear about this, but it seems
		// there's only occurrences of this command canceled.
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
		using namespace std::literals;
		static constexpr std::array cmds = {
			"time"sv, "realtime"sv, "idle"sv, "frame"sv, "break"sv, "boot"sv,
			"machine_switch"sv, "info"sv, "cancel"sv,
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
	auto p = partition_copy_remove(afterCmds, std::back_inserter(matches), pred);
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
	explicit AfterInputEventPred(AfterCommand::EventPtr event_)
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

AfterCmd::AfterCmd(AfterCommand& afterCommand_, TclObject command_)
	: afterCommand(afterCommand_), command(std::move(command_))
{
	ostringstream str;
	str << "after#" << ++lastAfterId;
	id = str.str();
}

string_view AfterCmd::getCommand() const
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
			"Error executing delayed command: ", e.getMessage());
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
		TclObject command_, double time_)
	: AfterCmd(afterCommand_, std::move(command_))
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
		TclObject command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, std::move(command_), time_)
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
		TclObject command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, std::move(command_), time_)
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
		TclObject command_)
	: AfterCmd(afterCommand_, std::move(command_)), type(type_.getString())
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
		TclObject command_)
	: AfterCmd(afterCommand_, std::move(command_))
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
		TclObject command_, double time)
	: AfterCmd(afterCommand_, std::move(command_))
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
