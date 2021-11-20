#include "Debugger.hh"
#include "Debuggable.hh"
#include "ProbeBreakPoint.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "BreakPoint.hh"
#include "DebugCondition.hh"
#include "MSXWatchIODevice.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MemBuffer.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "view.hh"
#include "xrange.hh"
#include <cassert>
#include <memory>

using std::string;
using std::string_view;

namespace openmsx {

Debugger::Debugger(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, cmd(motherBoard.getCommandController(),
	      motherBoard.getStateChangeDistributor(),
	      motherBoard.getScheduler())
{
}

Debugger::~Debugger()
{
	assert(!cpu);
	assert(debuggables.empty());
}

void Debugger::registerDebuggable(string name, Debuggable& debuggable)
{
	assert(!debuggables.contains(name));
	debuggables.emplace_noDuplicateCheck(std::move(name), &debuggable);
}

void Debugger::unregisterDebuggable(string_view name, Debuggable& debuggable)
{
	assert(debuggables.contains(name));
	assert(debuggables[name] == &debuggable); (void)debuggable;
	debuggables.erase(name);
}

Debuggable* Debugger::findDebuggable(string_view name)
{
	auto* v = lookup(debuggables, name);
	return v ? *v : nullptr;
}

Debuggable& Debugger::getDebuggable(string_view name)
{
	Debuggable* result = findDebuggable(name);
	if (!result) {
		throw CommandException("No such debuggable: ", name);
	}
	return *result;
}

void Debugger::registerProbe(ProbeBase& probe)
{
	assert(!probes.contains(probe.getName()));
	probes.insert_noDuplicateCheck(&probe);
}

void Debugger::unregisterProbe(ProbeBase& probe)
{
	assert(probes.contains(probe.getName()));
	probes.erase(probe.getName());
}

ProbeBase* Debugger::findProbe(string_view name)
{
	auto it = probes.find(name);
	return (it != std::end(probes)) ? *it : nullptr;
}

ProbeBase& Debugger::getProbe(string_view name)
{
	auto* result = findProbe(name);
	if (!result) {
		throw CommandException("No such probe: ", name);
	}
	return *result;
}

unsigned Debugger::insertProbeBreakPoint(
	TclObject command, TclObject condition,
	ProbeBase& probe, bool once, unsigned newId /*= -1*/)
{
	auto bp = std::make_unique<ProbeBreakPoint>(
		std::move(command), std::move(condition), *this, probe, once, newId);
	unsigned result = bp->getId();
	probeBreakPoints.push_back(std::move(bp));
	return result;
}

void Debugger::removeProbeBreakPoint(string_view name)
{
	if (StringOp::startsWith(name, "pp#")) {
		// remove by id
		if (auto id = StringOp::stringToBase<10, unsigned>(name.substr(3))) {
			if (auto it = ranges::find(probeBreakPoints, id, &ProbeBreakPoint::getId);
			    it != std::end(probeBreakPoints)) {
				move_pop_back(probeBreakPoints, it);
				return;
			}
		}
		throw CommandException("No such breakpoint: ", name);
	} else {
		// remove by probe, only works for unconditional bp
		auto it = ranges::find(probeBreakPoints, name, [](auto& bp) {
			return bp->getProbe().getName();
		});
		if (it == std::end(probeBreakPoints)) {
			throw CommandException(
				"No (unconditional) breakpoint for probe: ", name);
		}
		move_pop_back(probeBreakPoints, it);
	}
}

void Debugger::removeProbeBreakPoint(ProbeBreakPoint& bp)
{
	move_pop_back(probeBreakPoints, rfind_unguarded(probeBreakPoints, &bp,
		[](auto& v) { return v.get(); }));
}

unsigned Debugger::setWatchPoint(TclObject command, TclObject condition,
                                 WatchPoint::Type type,
                                 unsigned beginAddr, unsigned endAddr,
                                 bool once, unsigned newId /*= -1*/)
{
	std::shared_ptr<WatchPoint> wp;
	if (type == one_of(WatchPoint::READ_IO, WatchPoint::WRITE_IO)) {
		wp = std::make_shared<WatchIO>(
			motherBoard, type, beginAddr, endAddr,
			std::move(command), std::move(condition), once, newId);
	} else {
		wp = std::make_shared<WatchPoint>(
			std::move(command), std::move(condition), type, beginAddr, endAddr, once, newId);
	}
	motherBoard.getCPUInterface().setWatchPoint(wp);
	return wp->getId();
}

void Debugger::transfer(Debugger& other)
{
	// Copy watchpoints to new machine.
	assert(motherBoard.getCPUInterface().getWatchPoints().empty());
	for (const auto& wp : other.motherBoard.getCPUInterface().getWatchPoints()) {
		setWatchPoint(wp->getCommandObj(), wp->getConditionObj(),
		              wp->getType(),       wp->getBeginAddress(),
		              wp->getEndAddress(), wp->onlyOnce(),
		              wp->getId());
	}

	// Copy probes to new machine.
	assert(probeBreakPoints.empty());
	for (auto& bp : other.probeBreakPoints) {
		if (ProbeBase* probe = findProbe(bp->getProbe().getName())) {
			insertProbeBreakPoint(bp->getCommandObj(),
			                      bp->getConditionObj(),
			                      *probe, bp->onlyOnce(),
			                      bp->getId());
		}
	}

	// Breakpoints and conditions are (currently) global, so no need to
	// copy those.
}


// class Debugger::Cmd

static word getAddress(Interpreter& interp, const TclObject& token)
{
	unsigned addr = token.getInt(interp);
	if (addr >= 0x10000) {
		throw CommandException("Invalid address");
	}
	return addr;
}

Debugger::Cmd::Cmd(CommandController& commandController_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "debug")
{
}

bool Debugger::Cmd::needRecord(span<const TclObject> tokens) const
{
	// Note: it's crucial for security that only the write and write_block
	// subcommands are recorded and replayed. The 'set_bp' command for
	// example would allow to set a callback that can execute arbitrary Tcl
	// code. See comments in RecordedCommand for more details.
	if (tokens.size() < 2) return false;
	return tokens[1].getString() == one_of("write", "write_block");
}

void Debugger::Cmd::execute(
	span<const TclObject> tokens, TclObject& result, EmuTime::param /*time*/)
{
	checkNumArgs(tokens, AtLeast{2}, "subcommand ?arg ...?");
	executeSubCommand(tokens[1].getString(),
		"read",              [&]{ read(tokens, result); },
		"read_block",        [&]{ readBlock(tokens, result); },
		"write",             [&]{ write(tokens, result); },
		"write_block",       [&]{ writeBlock(tokens, result); },
		"size",              [&]{ size(tokens, result); },
		"desc",              [&]{ desc(tokens, result); },
		"list",              [&]{ list(result); },
		"step",              [&]{ debugger().motherBoard.getCPUInterface().doStep(); },
		"cont",              [&]{ debugger().motherBoard.getCPUInterface().doContinue(); },
		"disasm",            [&]{ debugger().cpu->disasmCommand(getInterpreter(), tokens, result); },
		"break",             [&]{ debugger().motherBoard.getCPUInterface().doBreak(); },
		"breaked",           [&]{ result = debugger().motherBoard.getCPUInterface().isBreaked(); },
		"set_bp",            [&]{ setBreakPoint(tokens, result); },
		"remove_bp",         [&]{ removeBreakPoint(tokens, result); },
		"list_bp",           [&]{ listBreakPoints(tokens, result); },
		"set_watchpoint",    [&]{ setWatchPoint(tokens, result); },
		"remove_watchpoint", [&]{ removeWatchPoint(tokens, result); },
		"list_watchpoints",  [&]{ listWatchPoints(tokens, result); },
		"set_condition",     [&]{ setCondition(tokens, result); },
		"remove_condition",  [&]{ removeCondition(tokens, result); },
		"list_conditions",   [&]{ listConditions(tokens, result); },
		"probe",             [&]{ probe(tokens, result); });
}

void Debugger::Cmd::list(TclObject& result)
{
	result.addListElements(view::keys(debugger().debuggables));
}

void Debugger::Cmd::desc(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "debuggable");
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	result = device.getDescription();
}

void Debugger::Cmd::size(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 3, "debuggable");
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	result = device.getSize();
}

void Debugger::Cmd::read(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, Prefix{2}, "debuggable address");
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned addr = tokens[3].getInt(getInterpreter());
	if (addr >= device.getSize()) {
		throw CommandException("Invalid address");
	}
	result = device.read(addr);
}

void Debugger::Cmd::readBlock(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 5, Prefix{2}, "debuggable address size");
	auto& interp = getInterpreter();
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned devSize = device.getSize();
	unsigned addr = tokens[3].getInt(interp);
	if (addr >= devSize) {
		throw CommandException("Invalid address");
	}
	unsigned num = tokens[4].getInt(interp);
	if (num > (devSize - addr)) {
		throw CommandException("Invalid size");
	}

	MemBuffer<byte> buf(num);
	for (auto i : xrange(num)) {
		buf[i] = device.read(addr + i);
	}
	result = span<byte>{buf.data(), num};
}

void Debugger::Cmd::write(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 5, Prefix{2}, "debuggable address value");
	auto& interp = getInterpreter();
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned addr = tokens[3].getInt(interp);
	if (addr >= device.getSize()) {
		throw CommandException("Invalid address");
	}
	unsigned value = tokens[4].getInt(interp);
	if (value >= 256) {
		throw CommandException("Invalid value");
	}

	device.write(addr, value);
}

void Debugger::Cmd::writeBlock(span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 5, Prefix{2}, "debuggable address values");
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned devSize = device.getSize();
	unsigned addr = tokens[3].getInt(getInterpreter());
	if (addr >= devSize) {
		throw CommandException("Invalid address");
	}
	auto buf = tokens[4].getBinary();
	if ((buf.size() + addr) > devSize) {
		throw CommandException("Invalid size");
	}

	for (auto i : xrange(buf.size())) {
		device.write(unsigned(addr + i), buf[i]);
	}
}

void Debugger::Cmd::setBreakPoint(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{3}, "address ?-once? ?condition? ?command?");
	TclObject command("debug break");
	TclObject condition;
	bool once = false;

	ArgsInfo info[] = { flagArg("-once", once) };
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(2), info);
	if ((arguments.size() < 1) || (arguments.size() > 3)) {
		throw SyntaxError();
	}

	switch (arguments.size()) {
	case 3: // command
		command = arguments[2];
		[[fallthrough]];
	case 2: // condition
		condition = arguments[1];
		[[fallthrough]];
	case 1: { // address
		word addr = getAddress(getInterpreter(), arguments[0]);
		BreakPoint bp(addr, command, condition, once);
		result = tmpStrCat("bp#", bp.getId());
		debugger().motherBoard.getCPUInterface().insertBreakPoint(std::move(bp));
		break;
	}
	}
}

void Debugger::Cmd::removeBreakPoint(
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 3, "id|address");
	auto& interface = debugger().motherBoard.getCPUInterface();
	auto& breakPoints = interface.getBreakPoints();

	string_view tmp = tokens[2].getString();
	if (StringOp::startsWith(tmp, "bp#")) {
		// remove by id
		if (auto id = StringOp::stringToBase<10, unsigned>(tmp.substr(3))) {
			if (auto it = ranges::find(breakPoints, id, &BreakPoint::getId);
			    it != std::end(breakPoints)) {
				interface.removeBreakPoint(*it);
				return;
			}
		}
		throw CommandException("No such breakpoint: ", tmp);
	} else {
		// remove by addr, only works for unconditional bp
		word addr = getAddress(getInterpreter(), tokens[2]);
		auto [first, last] = ranges::equal_range(breakPoints, addr, {}, &BreakPoint::getAddress);
		auto it = std::find_if(first, last, [](auto& bp) {
			return bp.getCondition().empty();
		});
		if (it == last) {
			throw CommandException(
				"No (unconditional) breakpoint at address: ", tmp);
		}
		interface.removeBreakPoint(*it);
	}
}

void Debugger::Cmd::listBreakPoints(
	span<const TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (const auto& bp : interface.getBreakPoints()) {
		TclObject line = makeTclList(
			tmpStrCat("bp#", bp.getId()),
			tmpStrCat("0x", hex_string<4>(bp.getAddress())),
			bp.getCondition(),
			bp.getCommand());
		strAppend(res, line.getString(), '\n');
	}
	result = res;
}


void Debugger::Cmd::setWatchPoint(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{4}, Prefix{2}, "type address ?-once? ?condition? ?command?");
	TclObject command("debug break");
	TclObject condition;
	unsigned beginAddr, endAddr;
	WatchPoint::Type type;
	bool once = false;

	ArgsInfo info[] = { flagArg("-once", once) };
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(2), info);
	if ((arguments.size() < 2) || (arguments.size() > 4)) {
		throw SyntaxError();
	}

	switch (arguments.size()) {
	case 4: // command
		command = arguments[3];
		[[fallthrough]];
	case 3: // condition
		condition = arguments[2];
		[[fallthrough]];
	case 2: { // address + type
		string_view typeStr = arguments[0].getString();
		unsigned max = [&] {
			if (typeStr == "read_io") {
				type = WatchPoint::READ_IO;
				return 0x100;
			} else if (typeStr == "write_io") {
				type = WatchPoint::WRITE_IO;
				return 0x100;
			} else if (typeStr == "read_mem") {
				type = WatchPoint::READ_MEM;
				return 0x10000;
			} else if (typeStr == "write_mem") {
				type = WatchPoint::WRITE_MEM;
				return 0x10000;
			} else {
				throw CommandException("Invalid type: ", typeStr);
			}
		}();
		auto& interp = getInterpreter();
		if (arguments[1].getListLength(interp) == 2) {
			beginAddr = arguments[1].getListIndex(interp, 0).getInt(interp);
			endAddr   = arguments[1].getListIndex(interp, 1).getInt(interp);
			if (endAddr < beginAddr) {
				throw CommandException(
					"Not a valid range: end address may "
					"not be smaller than begin address.");
			}
		} else {
			beginAddr = endAddr = arguments[1].getInt(interp);
		}
		if (endAddr >= max) {
			throw CommandException("Invalid address: out of range");
		}
		break;
	}
	default:
		UNREACHABLE; break;
	}
	unsigned id = debugger().setWatchPoint(
		command, condition, type, beginAddr, endAddr, once);
	result = tmpStrCat("wp#", id);
}

void Debugger::Cmd::removeWatchPoint(
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 3, "id");
	string_view tmp = tokens[2].getString();
	if (StringOp::startsWith(tmp, "wp#")) {
		// remove by id
		if (auto id = StringOp::stringToBase<10, unsigned>(tmp.substr(3))) {
			auto& interface = debugger().motherBoard.getCPUInterface();
			for (auto& wp : interface.getWatchPoints()) {
				if (wp->getId() == *id) {
					interface.removeWatchPoint(wp);
					return;
				}
			}
		}
	}
	throw CommandException("No such watchpoint: ", tmp);
}

void Debugger::Cmd::listWatchPoints(
	span<const TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (const auto& wp : interface.getWatchPoints()) {
		TclObject line = makeTclList(tmpStrCat("wp#", wp->getId()));
		string type;
		switch (wp->getType()) {
		case WatchPoint::READ_IO:
			type = "read_io";
			break;
		case WatchPoint::WRITE_IO:
			type = "write_io";
			break;
		case WatchPoint::READ_MEM:
			type = "read_mem";
			break;
		case WatchPoint::WRITE_MEM:
			type = "write_mem";
			break;
		default:
			UNREACHABLE; break;
		}
		line.addListElement(type);
		unsigned beginAddr = wp->getBeginAddress();
		unsigned endAddr   = wp->getEndAddress();
		if (beginAddr == endAddr) {
			line.addListElement(tmpStrCat("0x", hex_string<4>(beginAddr)));
		} else {
			line.addListElement(makeTclList(
				tmpStrCat("0x", hex_string<4>(beginAddr)),
				tmpStrCat("0x", hex_string<4>(endAddr))));
		}
		line.addListElement(wp->getCondition(),
		                    wp->getCommand());
		strAppend(res, line.getString(), '\n');
	}
	result = res;
}


void Debugger::Cmd::setCondition(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{3}, "condition ?-once? ?command?");
	TclObject command("debug break");
	TclObject condition;
	bool once = false;

	ArgsInfo info[] = { flagArg("-once", once) };
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(2), info);
	if ((arguments.size() < 1) || (arguments.size() > 2)) {
		throw SyntaxError();
	}

	switch (arguments.size()) {
	case 2: // command
		command = arguments[1];
		[[fallthrough]];
	case 1: { // condition
		condition = arguments[0];
		DebugCondition dc(command, condition, once);
		result = tmpStrCat("cond#", dc.getId());
		debugger().motherBoard.getCPUInterface().setCondition(std::move(dc));
		break;
	}
	}
}

void Debugger::Cmd::removeCondition(
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 3, "id");

	string_view tmp = tokens[2].getString();
	if (StringOp::startsWith(tmp, "cond#")) {
		// remove by id
		if (auto id = StringOp::stringToBase<10, unsigned>(tmp.substr(5))) {
			auto& interface = debugger().motherBoard.getCPUInterface();
			for (auto& c : interface.getConditions()) {
				if (c.getId() == *id) {
					interface.removeCondition(c);
					return;
				}
			}
		}
	}
	throw CommandException("No such condition: ", tmp);
}

void Debugger::Cmd::listConditions(
	span<const TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (const auto& c : interface.getConditions()) {
		TclObject line = makeTclList(tmpStrCat("cond#", c.getId()),
		                             c.getCondition(),
		                             c.getCommand());
		strAppend(res, line.getString(), '\n');
	}
	result = res;
}


void Debugger::Cmd::probe(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{3}, "subcommand ?arg ...?");
	executeSubCommand(tokens[2].getString(),
		"list",      [&]{ probeList(tokens, result); },
		"desc",      [&]{ probeDesc(tokens, result); },
		"read",      [&]{ probeRead(tokens, result); },
		"set_bp",    [&]{ probeSetBreakPoint(tokens, result); },
		"remove_bp", [&]{ probeRemoveBreakPoint(tokens, result); },
		"list_bp",   [&]{ probeListBreakPoints(tokens, result); });
}
void Debugger::Cmd::probeList(span<const TclObject> /*tokens*/, TclObject& result)
{
	result.addListElements(view::transform(debugger().probes,
		[](auto* p) { return p->getName(); }));
}
void Debugger::Cmd::probeDesc(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, "probe");
	result = debugger().getProbe(tokens[3].getString()).getDescription();
}
void Debugger::Cmd::probeRead(span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 4, "probe");
	result = debugger().getProbe(tokens[3].getString()).getValue();
}
void Debugger::Cmd::probeSetBreakPoint(
	span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{4}, "probe ?-once? ?condition? ?command?");
	TclObject command("debug break");
	TclObject condition;
	ProbeBase* p;
	bool once = false;

	ArgsInfo info[] = { flagArg("-once", once) };
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(3), info);
	if ((arguments.size() < 1) || (arguments.size() > 3)) {
		throw SyntaxError();
	}

	switch (arguments.size()) {
	case 3: // command
		command = arguments[2];
		[[fallthrough]];
	case 2: // condition
		condition = arguments[1];
		[[fallthrough]];
	case 1: { // probe
		p = &debugger().getProbe(arguments[0].getString());
		break;
	}
	default:
		UNREACHABLE; break;
	}

	unsigned id = debugger().insertProbeBreakPoint(command, condition, *p, once);
	result = tmpStrCat("pp#", id);
}
void Debugger::Cmd::probeRemoveBreakPoint(
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 4, "id");
	debugger().removeProbeBreakPoint(tokens[3].getString());
}
void Debugger::Cmd::probeListBreakPoints(
	span<const TclObject> /*tokens*/, TclObject& result)
{
	string res;
	for (auto& p : debugger().probeBreakPoints) {
		TclObject line = makeTclList(tmpStrCat("pp#", p->getId()),
		                             p->getProbe().getName(),
		                             p->getCondition(),
		                             p->getCommand());
		strAppend(res, line.getString(), '\n');
	}
	result = res;
}

string Debugger::Cmd::help(span<const TclObject> tokens) const
{
	auto generalHelp =
		"debug <subcommand> [<arguments>]\n"
		"  Possible subcommands are:\n"
		"    list              returns a list of all debuggables\n"
		"    desc              returns a description of this debuggable\n"
		"    size              returns the size of this debuggable\n"
		"    read              read a byte from a debuggable\n"
		"    write             write a byte to a debuggable\n"
		"    read_block        read a whole block at once\n"
		"    write_block       write a whole block at once\n"
		"    set_bp            insert a new breakpoint\n"
		"    remove_bp         remove a certain breakpoint\n"
		"    list_bp           list the active breakpoints\n"
		"    set_watchpoint    insert a new watchpoint\n"
		"    remove_watchpoint remove a certain watchpoint\n"
		"    list_watchpoints  list the active watchpoints\n"
		"    set_condition     insert a new condition\n"
		"    remove_condition  remove a certain condition\n"
		"    list_conditions   list the active conditions\n"
		"    probe             probe related subcommands\n"
		"    cont              continue execution after break\n"
		"    step              execute one instruction\n"
		"    break             break CPU at current position\n"
		"    breaked           query CPU breaked status\n"
		"    disasm            disassemble instructions\n"
		"  The arguments are specific for each subcommand.\n"
		"  Type 'help debug <subcommand>' for help about a specific subcommand.\n";

	auto listHelp =
		"debug list\n"
		"  Returns a list with the names of all 'debuggables'.\n"
		"  These names are used in other debug subcommands.\n";
	auto descHelp =
		"debug desc <name>\n"
		"  Returns a description for the debuggable with given name.\n";
	auto sizeHelp =
		"debug size <name>\n"
		"  Returns the size (in bytes) of the debuggable with given name.\n";
	auto readHelp =
		"debug read <name> <addr>\n"
		"  Read a byte at offset <addr> from the given debuggable.\n"
		"  The offset must be smaller than the value returned from the "
		"'size' subcommand\n"
		"  Note that openMSX comes with a bunch of Tcl scripts that make "
		"some of the debug reads much more convenient (e.g. reading from "
		"Z80 or VDP registers). See the Console Command Reference for more "
		"details about these.\n";
	auto writeHelp =
		"debug write <name> <addr> <val>\n"
		"  Write a byte to the given debuggable at a certain offset.\n"
		"  The offset must be smaller than the value returned from the "
		"'size' subcommand\n";
	auto readBlockHelp =
		"debug read_block <name> <addr> <size>\n"
		"  Read a whole block at once. This is equivalent with repeated "
		"invocations of the 'read' subcommand, but using this subcommand "
		"may be faster. The result is a Tcl binary string (see Tcl manual).\n"
		"  The block is specified as size/offset in the debuggable. The "
		"complete block must fit in the debuggable (see the 'size' "
		"subcommand).\n";
	auto writeBlockHelp =
		"debug write_block <name> <addr> <values>\n"
		"  Write a whole block at once. This is equivalent with repeated "
		"invocations of the 'write' subcommand, but using this subcommand "
		"may be faster. The <values> argument must be a Tcl binary string "
		"(see Tcl manual).\n"
		"  The block has a size and an offset in the debuggable. The "
		"complete block must fit in the debuggable (see the 'size' "
		"subcommand).\n";
	auto setBpHelp =
		"debug set_bp [-once] <addr> [<cond>] [<cmd>]\n"
		"  Insert a new breakpoint at given address. When the CPU is about "
		"to execute the instruction at this address, execution will be "
		"breaked. At least this is the default behaviour, see next "
		"paragraphs.\n"
		"  When the -once flag is given, the breakpoint is automatically "
		"removed after it triggered. In other words: it only triggers once.\n"
		"  Optionally you can specify a condition. When the CPU reaches "
		"the breakpoint this condition is evaluated, only when the condition "
		"evaluated to true execution will be breaked.\n"
		"  A condition must be specified as a Tcl expression. For example\n"
		"     debug set_bp 0xf37d {[reg C] == 0x2F}\n"
		"  This breaks on address 0xf37d but only when Z80 register C has the "
		"value 0x2F.\n"
		"  Also optionally you can specify a command that should be "
		"executed when the breakpoint is reached (and condition is true). "
		"By default this command is 'debug break'.\n"
		"  The result of this command is a breakpoint ID. This ID can "
		"later be used to remove this breakpoint again.\n";
	auto removeBpHelp =
		"debug remove_bp <id>\n"
		"  Remove the breakpoint with given ID again. You can use the "
		"'list_bp' subcommand to see all valid IDs.\n";
	auto listBpHelp =
		"debug list_bp\n"
		"  Lists all active breakpoints. The result is printed in 4 "
		"columns. The first column contains the breakpoint ID. The "
		"second one has the address. The third has the condition "
		"(default condition is empty). And the last column contains "
		"the command that will be executed (default is 'debug break').\n";
	auto setWatchPointHelp =
		"debug set_watchpoint [-once] <type> <region> [<cond>] [<cmd>]\n"
		"  Insert a new watchpoint of given type on the given region, "
		"there can be an optional -once flag, a condition and alternative "
		"command. See the 'set_bp' subcommand for details about these.\n"
		"  Type must be one of the following:\n"
		"    read_io    break when CPU reads from given IO port(s)\n"
		"    write_io   break when CPU writes to given IO port(s)\n"
		"    read_mem   break when CPU reads from given memory location(s)\n"
		"    write_mem  break when CPU writes to given memory location(s)\n"
		"  Region is either a single value, this corresponds to a single "
		"memory location or IO port. Otherwise region must be a list of "
		"two values (enclosed in braces) that specify a begin and end "
		"point of a whole memory region or a range of IO ports.\n"
		"During the execution of <cmd>, the following global Tcl "
		"variables are set:\n"
		"  ::wp_last_address   this is the actual address of the mem/io "
		"read/write that triggered the watchpoint\n"
		"  ::wp_last_value     this is the actual value that was written "
		"by the mem/io write that triggered the watchpoint\n"
		"Examples:\n"
		"  debug set_watchpoint write_io 0x99 {[reg A] == 0x81}\n"
		"  debug set_watchpoint read_mem {0xfbe5 0xfbef}\n";
	auto removeWatchPointHelp =
		"debug remove_watchpoint <id>\n"
		"  Remove the watchpoint with given ID again. You can use the "
		"'list_watchpoints' subcommand to see all valid IDs.\n";
	auto listWatchPointsHelp =
		"debug list_watchpoints\n"
		"  Lists all active watchpoints. The result is similar to the "
		"'list_bp' subcommand, but there is an extra column (2nd column) "
		"that contains the type of the watchpoint.\n";
	auto setCondHelp =
		"debug set_condition [-once] <cond> [<cmd>]\n"
		"  Insert a new condition. These are much like breakpoints, "
		"except that they are checked before every instruction "
		"(breakpoints are tied to a specific address).\n"
		"  Conditions will slow down simulation MUCH more than "
		"breakpoints. So only use them when you don't care about "
		"simulation speed (when you're debugging this is usually not "
		"a problem).\n"
		"  See 'help debug set_bp' for more details.\n";
	auto removeCondHelp =
		"debug remove_condition <id>\n"
		"  Remove the condition with given ID again. You can use the "
		"'list_conditions' subcommand to see all valid IDs.\n";
	auto listCondHelp =
		"debug list_conditions\n"
		"  Lists all active conditions. The result is similar to the "
		"'list_bp' subcommand, but without the 2nd column that would "
		"show the address.\n";
	auto probeHelp =
		"debug probe <subcommand> [<arguments>]\n"
		"  Possible subcommands are:\n"
		"    list                                     returns a list of all probes\n"
		"    desc   <probe>                           returns a description of this probe\n"
		"    read   <probe>                           returns the current value of this probe\n"
		"    set_bp <probe> [-once] [<cond>] [<cmd>]  set a breakpoint on the given probe\n"
		"    remove_bp <id>                           remove the given breakpoint\n"
		"    list_bp                                  returns a list of breakpoints that are set on probes\n";
	auto contHelp =
		"debug cont\n"
		"  Continue execution after CPU was breaked.\n";
	auto stepHelp =
		"debug step\n"
		"  Execute one instruction. This command is only meaningful in "
		"break mode.\n";
	auto breakHelp =
		"debug break\n"
		"  Immediately break CPU execution. When CPU was already breaked "
		"this command has no effect.\n";
	auto breakedHelp =
		"debug breaked\n"
		"  Query the CPU breaked status. Returns '1' when CPU was "
		"breaked, '0' otherwise.\n";
	auto disasmHelp =
		"debug disasm <addr>\n"
		"  Disassemble the instruction at the given address. The result "
		"is a Tcl list. The first element in the list contains a textual "
		"representation of the instruction, the next elements contain the "
		"bytes that make up this instruction (thus the length of the "
		"resulting list can be used to derive the number of bytes in the "
		"instruction).\n"
		"  Note that openMSX comes with a 'disasm' Tcl script that is much "
		"more convenient to use than this subcommand.";
	auto unknownHelp =
		"Unknown subcommand, use 'help debug' to see a list of valid "
		"subcommands.\n";

	if (tokens.size() == 1) {
		return generalHelp;
	} else if (tokens[1] == "list") {
		return listHelp;
	} else if (tokens[1] == "desc") {
		return descHelp;
	} else if (tokens[1] == "size") {
		return sizeHelp;
	} else if (tokens[1] == "read") {
		return readHelp;
	} else if (tokens[1] == "write") {
		return writeHelp;
	} else if (tokens[1] == "read_block") {
		return readBlockHelp;
	} else if (tokens[1] == "write_block") {
		return writeBlockHelp;
	} else if (tokens[1] == "set_bp") {
		return setBpHelp;
	} else if (tokens[1] == "remove_bp") {
		return removeBpHelp;
	} else if (tokens[1] == "list_bp") {
		return listBpHelp;
	} else if (tokens[1] == "set_watchpoint") {
		return setWatchPointHelp;
	} else if (tokens[1] == "remove_watchpoint") {
		return removeWatchPointHelp;
	} else if (tokens[1] == "list_watchpoints") {
		return listWatchPointsHelp;
	} else if (tokens[1] == "set_condition") {
		return setCondHelp;
	} else if (tokens[1] == "remove_condition") {
		return removeCondHelp;
	} else if (tokens[1] == "list_conditions") {
		return listCondHelp;
	} else if (tokens[1] == "probe") {
		return probeHelp;
	} else if (tokens[1] == "cont") {
		return contHelp;
	} else if (tokens[1] == "step") {
		return stepHelp;
	} else if (tokens[1] == "break") {
		return breakHelp;
	} else if (tokens[1] == "breaked") {
		return breakedHelp;
	} else if (tokens[1] == "disasm") {
		return disasmHelp;
	} else {
		return unknownHelp;
	}
}

std::vector<string> Debugger::Cmd::getBreakPointIds() const
{
	return to_vector(view::transform(
		debugger().motherBoard.getCPUInterface().getBreakPoints(),
		[](auto& bp) { return strCat("bp#", bp.getId()); }));
}
std::vector<string> Debugger::Cmd::getWatchPointIds() const
{
	return to_vector(view::transform(
		debugger().motherBoard.getCPUInterface().getWatchPoints(),
		[](auto& w) { return strCat("wp#", w->getId()); }));
}
std::vector<string> Debugger::Cmd::getConditionIds() const
{
	return to_vector(view::transform(
		debugger().motherBoard.getCPUInterface().getConditions(),
		[](auto& c) { return strCat("cond#", c.getId()); }));
}

void Debugger::Cmd::tabCompletion(std::vector<string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array singleArgCmds = {
		"list"sv, "step"sv, "cont"sv, "break"sv, "breaked"sv,
		"list_bp"sv, "list_watchpoints"sv, "list_conditions"sv,
	};
	static constexpr std::array debuggableArgCmds = {
		"desc"sv, "size"sv, "read"sv, "read_block"sv,
		"write"sv, "write_block"sv,
	};
	static constexpr std::array otherCmds = {
		"disasm"sv, "set_bp"sv, "remove_bp"sv, "set_watchpoint"sv,
		"remove_watchpoint"sv, "set_condition"sv, "remove_condition"sv,
		"probe"sv,
	};
	switch (tokens.size()) {
	case 2: {
		completeString(tokens, concatArray(singleArgCmds, debuggableArgCmds, otherCmds));
		break;
	}
	case 3:
		if (!contains(singleArgCmds, tokens[1])) {
			// this command takes (an) argument(s)
			if (contains(debuggableArgCmds, tokens[1])) {
				// it takes a debuggable here
				completeString(tokens, view::keys(debugger().debuggables));
			} else if (tokens[1] == "remove_bp") {
				// this one takes a bp id
				completeString(tokens, getBreakPointIds());
			} else if (tokens[1] == "remove_watchpoint") {
				// this one takes a wp id
				completeString(tokens, getWatchPointIds());
			} else if (tokens[1] == "remove_condition") {
				// this one takes a cond id
				completeString(tokens, getConditionIds());
			} else if (tokens[1] == "set_watchpoint") {
				static constexpr std::array types = {
					"write_io"sv, "write_mem"sv,
					"read_io"sv, "read_mem"sv,
				};
				completeString(tokens, types);
			} else if (tokens[1] == "probe") {
				static constexpr std::array subCmds = {
					"list"sv, "desc"sv, "read"sv, "set_bp"sv,
					"remove_bp"sv, "list_bp"sv,
				};
				completeString(tokens, subCmds);
			}
		}
		break;
	case 4:
		if ((tokens[1] == "probe") &&
		    (tokens[2] == one_of("desc", "read", "set_bp"))) {
			completeString(tokens, view::transform(
				debugger().probes,
				[](auto* p) -> std::string_view { return p->getName(); }));
		}
		break;
	}
}

} // namespace openmsx
