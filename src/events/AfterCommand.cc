#include "AfterCommand.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "Event.hh"
#include "Schedulable.hh"
#include "EventDistributor.hh"
#include "InputEventFactory.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "ObjectPool.hh"
#include "RTSchedulable.hh"
#include "EmuTime.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "TclObject.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "view.hh"
#include <iterator>
#include <memory>
#include <sstream>
#include <variant>

using std::string;
using std::string_view;
using std::vector;

namespace openmsx {

class AfterCmd
{
public:
	[[nodiscard]] const auto& getCommand() const { return command; }
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] auto getIdStr() const { return tmpStrCat("after#", id); }
	void execute();
protected:
	AfterCmd(AfterCommand& afterCommand,
		 TclObject command);
	AfterCommand::Index removeSelf();

	AfterCommand& afterCommand;
	TclObject command;
	const unsigned id;
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
};

class AfterIdleCmd final : public AfterTimedCmd
{
public:
	AfterIdleCmd(Scheduler& scheduler,
		     AfterCommand& afterCommand,
		     TclObject command, double time);
};

class AfterSimpleEventCmd final : public AfterCmd
{
public:
	AfterSimpleEventCmd(AfterCommand& afterCommand,
	                    TclObject command,
	                    EventType type);
	[[nodiscard]] string_view getType() const;
	[[nodiscard]] EventType getTypeEnum() const { return type; }
private:
	EventType type;
};

class AfterInputEventCmd final : public AfterCmd
{
public:
	AfterInputEventCmd(AfterCommand& afterCommand,
	                   Event event,
	                   TclObject command);
	[[nodiscard]] const Event& getEvent() const { return event; }
private:
	Event event;
};

class AfterRealTimeCmd final : public AfterCmd, private RTSchedulable
{
public:
	AfterRealTimeCmd(RTScheduler& rtScheduler, AfterCommand& afterCommand,
	                 TclObject command, double time);

private:
	void executeRT() override;
};

using AllAfterCmds = std::variant<AfterTimeCmd,
                                  AfterIdleCmd,
                                  AfterSimpleEventCmd,
                                  AfterInputEventCmd,
                                  AfterRealTimeCmd>;
static ObjectPool<AllAfterCmds> afterCmdPool;


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
		EventType::KEY_UP, *this);
	eventDistributor.registerEventListener(
		EventType::KEY_DOWN, *this);
	eventDistributor.registerEventListener(
		EventType::MOUSE_MOTION, *this);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_UP, *this);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_DOWN, *this);
	eventDistributor.registerEventListener(
		EventType::MOUSE_WHEEL, *this);
	eventDistributor.registerEventListener(
		EventType::JOY_AXIS_MOTION, *this);
	eventDistributor.registerEventListener(
		EventType::JOY_HAT, *this);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_UP, *this);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_DOWN, *this);
	eventDistributor.registerEventListener(
		EventType::FINISH_FRAME, *this);
	eventDistributor.registerEventListener(
		EventType::BREAK, *this);
	eventDistributor.registerEventListener(
		EventType::QUIT, *this);
	eventDistributor.registerEventListener(
		EventType::BOOT, *this);
	eventDistributor.registerEventListener(
		EventType::MACHINE_LOADED, *this);
	eventDistributor.registerEventListener(
		EventType::AFTER_TIMED, *this);
}

AfterCommand::~AfterCommand()
{
	for (auto idx : afterCmds) {
		afterCmdPool.remove(idx);
	}

	eventDistributor.unregisterEventListener(
		EventType::AFTER_TIMED, *this);
	eventDistributor.unregisterEventListener(
		EventType::MACHINE_LOADED, *this);
	eventDistributor.unregisterEventListener(
		EventType::BOOT, *this);
	eventDistributor.unregisterEventListener(
		EventType::QUIT, *this);
	eventDistributor.unregisterEventListener(
		EventType::BREAK, *this);
	eventDistributor.unregisterEventListener(
		EventType::FINISH_FRAME, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_BUTTON_UP, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_HAT, *this);
	eventDistributor.unregisterEventListener(
		EventType::JOY_AXIS_MOTION, *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_WHEEL, *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_BUTTON_UP, *this);
	eventDistributor.unregisterEventListener(
		EventType::MOUSE_MOTION, *this);
	eventDistributor.unregisterEventListener(
		EventType::KEY_DOWN, *this);
	eventDistributor.unregisterEventListener(
		EventType::KEY_UP, *this);
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
		afterSimpleEvent(tokens, result, EventType::FINISH_FRAME);
	} else if (subCmd == "break") {
		afterSimpleEvent(tokens, result, EventType::BREAK);
	} else if (subCmd == "quit") {
		afterSimpleEvent(tokens, result, EventType::QUIT);
	} else if (subCmd == "boot") {
		afterSimpleEvent(tokens, result, EventType::BOOT);
	} else if (subCmd == "machine_switch") {
		afterSimpleEvent(tokens, result, EventType::MACHINE_LOADED);
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
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterTimeCmd>{},
		motherBoard->getScheduler(), *this, tokens[3], time);
	result = std::get<AfterTimeCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterRealTime(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "seconds command");
	double time = getTime(getInterpreter(), tokens[2]);
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterRealTimeCmd>{},
		reactor.getRTScheduler(), *this, tokens[3], time);
	result = std::get<AfterRealTimeCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterTclTime(
	int ms, span<const TclObject> tokens, TclObject& result)
{
	TclObject command;
	command.addListElements(view::drop(tokens, 2));
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterRealTimeCmd>{},
		reactor.getRTScheduler(), *this, command, ms / 1000.0);
	result = std::get<AfterRealTimeCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterSimpleEvent(span<const TclObject> tokens, TclObject& result, EventType type)
{
	checkNumArgs(tokens, 3, "command");
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterSimpleEventCmd>{},
		*this, tokens[2], type);
	result = std::get<AfterSimpleEventCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterInputEvent(
	const Event& event, span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "command");
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterInputEventCmd>{},
		*this, event, tokens[2]);
	result = std::get<AfterInputEventCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterIdle(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "seconds command");
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return;
	double time = getTime(getInterpreter(), tokens[2]);
	auto [idx, ptr] = afterCmdPool.emplace(
		std::in_place_type_t<AfterIdleCmd>{},
		motherBoard->getScheduler(), *this, tokens[3], time);
	result = std::get<AfterIdleCmd>(*ptr).getIdStr();
	afterCmds.push_back(idx);
}

void AfterCommand::afterInfo(span<const TclObject> /*tokens*/, TclObject& result)
{
	auto printTime = [](std::ostream& os, const AfterTimedCmd& cmd) {
		os.precision(3);
		os << std::fixed << std::showpoint << cmd.getTime() << ' ';
	};

	std::ostringstream str;
	for (auto idx : afterCmds) {
		auto& var = afterCmdPool[idx];
		std::visit([&](AfterCmd& cmd) { str << cmd.getIdStr() << ": "; }, var);
		std::visit(overloaded {
			[&](AfterTimeCmd&        cmd ) { str << "time "; printTime(str, cmd); },
			[&](AfterIdleCmd&        cmd ) { str << "idle "; printTime(str, cmd); },
			[&](AfterSimpleEventCmd& cmd ) { str << cmd.getType() << ' '; },
			[&](AfterInputEventCmd&  cmd ) { str << toString(cmd.getEvent()) << ' '; },
			[&](AfterRealTimeCmd& /*cmd*/) { str << "realtime "; }
		}, var);
		std::visit([&](AfterCmd& cmd) { str << cmd.getCommand().getString() << '\n'; }, var);
	}
	result = str.str();
}

void AfterCommand::afterCancel(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, AtLeast{3}, "id|command");
	if (tokens.size() == 3) {
		if (auto idStr = tokens[2].getString(); StringOp::startsWith(idStr, "after#")) {
			if (auto id = StringOp::stringTo<unsigned>(idStr.substr(6))) {
				auto equalId = [id = *id](Index idx) {
					return std::visit([&](AfterCmd& cmd) {
						 return cmd.getId() == id;
					}, afterCmdPool[idx]);
				};
				if (auto it = ranges::find_if(afterCmds, equalId);
				    it != end(afterCmds)) {
					auto idx = *it;
					afterCmds.erase(it);
					afterCmdPool.remove(idx);
					return;
				}
			}
		}
	}
	TclObject command;
	command.addListElements(view::drop(tokens, 2));
	string_view cmdStr = command.getString();
	auto equalCmd = [&](Index idx) {
		return std::visit([&](AfterCmd& cmd) {
			return cmd.getCommand() == cmdStr;
		}, afterCmdPool[idx]);
	};
	if (auto it = ranges::find_if(afterCmds, equalCmd);
	    it != end(afterCmds)) {
		auto idx = *it;
		afterCmds.erase(it);
		afterCmdPool.remove(idx);
		// Tcl manual is not clear about this, but it seems
		// there's only occurrences of this command canceled.
		// It's also not clear which of the (possibly) several
		// matches is canceled.
		return;
	}
	// It's not an error if no match is found
}

string AfterCommand::help(span<const TclObject> /*tokens*/) const
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
	static std::vector<Index> matches; // static to keep capacity for next call
	assert(matches.empty());

	// Usually there are very few matches (typically even 0 or 1), so no
	// need to reserve() space.
	auto p = partition_copy_remove(afterCmds, std::back_inserter(matches), pred);
	afterCmds.erase(p.second, end(afterCmds));
	for (auto idx : matches) {
		std::visit([](AfterCmd& cmd) { cmd.execute(); },
		           afterCmdPool[idx]);
		afterCmdPool.remove(idx);
	}
	matches.clear(); // for next call (but keep capacity)
}

struct AfterSimpleEventPred {
	bool operator()(AfterCommand::Index idx) const {
		return std::visit(overloaded {
			[&](AfterSimpleEventCmd& cmd) { return cmd.getTypeEnum() == type; },
			[&](AfterCmd& /*cmd*/) { return false; }
		}, afterCmdPool[idx]);
	}
	const EventType type;
};
void AfterCommand::executeSimpleEvents(EventType type)
{
	executeMatches(AfterSimpleEventPred{type});
}

struct AfterEmuTimePred {
	bool operator()(AfterCommand::Index idx) const {
		return std::visit(overloaded {
			[&](AfterTimedCmd& cmd) { return cmd.getTime() == 0.0; },
			[&](AfterCmd& /*cmd*/) { return false; }
		}, afterCmdPool[idx]);
	}
};

struct AfterInputEventPred {
	explicit AfterInputEventPred(Event event_)
		: event(std::move(event_)) {}
	bool operator()(AfterCommand::Index idx) const {
		return std::visit(overloaded {
			[&](AfterInputEventCmd& cmd) { return matches(cmd.getEvent(), event); },
			[&](AfterCmd& /*cmd*/) { return false; }
		}, afterCmdPool[idx]);
	}
	Event event;
};

int AfterCommand::signalEvent(const Event& event) noexcept
{
	visit(overloaded{
		[&](const SimpleEvent&) {
			executeSimpleEvents(getType(event));
		},
		[&](const AfterTimedEvent&) {
			executeMatches(AfterEmuTimePred());
		},
		[&](const EventBase&) {
			executeMatches(AfterInputEventPred(event));
			for (auto idx : afterCmds) {
				std::visit(overloaded {
					[](AfterIdleCmd& cmd) { cmd.reschedule(); },
					[](AfterCmd& /*cmd*/) { /*nothing*/ }
				}, afterCmdPool[idx]);
			}
		}
	}, event);
	return 0;
}


// class AfterCmd

AfterCmd::AfterCmd(AfterCommand& afterCommand_, TclObject command_)
	: afterCommand(afterCommand_), command(std::move(command_)), id(++lastAfterId)
{
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

AfterCommand::Index AfterCmd::removeSelf()
{
	auto equalThis = [&](AfterCommand::Index idx) {
		return std::visit([&](AfterCmd& cmd) { return &cmd == this; },
			          afterCmdPool[idx]);
	};
	auto it = rfind_if_unguarded(afterCommand.afterCmds, equalThis);
	auto idx = *it;
	afterCommand.afterCmds.erase(it); // move_pop_back ?
	return idx;
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
		Event::create<AfterTimedEvent>());
}

void AfterTimedCmd::schedulerDeleted()
{
	auto idx = removeSelf();
	afterCmdPool.remove(idx);
}


// class AfterTimeCmd

AfterTimeCmd::AfterTimeCmd(
		Scheduler& scheduler_,
		AfterCommand& afterCommand_,
		TclObject command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, std::move(command_), time_)
{
}


// class AfterIdleCmd

AfterIdleCmd::AfterIdleCmd(
		Scheduler& scheduler_,
		AfterCommand& afterCommand_,
		TclObject command_, double time_)
	: AfterTimedCmd(scheduler_, afterCommand_, std::move(command_), time_)
{
}


// class AfterSimpleEventCmd

AfterSimpleEventCmd::AfterSimpleEventCmd(
		AfterCommand& afterCommand_, TclObject command_,
		EventType type_)
	: AfterCmd(afterCommand_, std::move(command_)), type(type_)
{
}

string_view AfterSimpleEventCmd::getType() const
{
	switch (type) {
		case EventType::FINISH_FRAME:   return "frame";
		case EventType::BREAK:          return "break";
		case EventType::BOOT:           return "boot";
		case EventType::QUIT:           return "quit";
		case EventType::MACHINE_LOADED: return "machine_switch";
		default: UNREACHABLE;           return "";
	}
}


// AfterInputEventCmd

AfterInputEventCmd::AfterInputEventCmd(
		AfterCommand& afterCommand_,
		Event event_,
		TclObject command_)
	: AfterCmd(afterCommand_, std::move(command_))
	, event(std::move(event_))
{
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

void AfterRealTimeCmd::executeRT()
{
	// Remove self before executing, but keep self alive till the end of
	// this method. Otherwise execute could execute 'after cancel ..' and
	// removeSelf() asserts that it can't find itself anymore.
	auto idx = removeSelf();
	execute();
	afterCmdPool.remove(idx);
}

} // namespace openmsx
