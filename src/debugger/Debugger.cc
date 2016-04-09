#include "Debugger.hh"
#include "Debuggable.hh"
#include "ProbeBreakPoint.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "BreakPoint.hh"
#include "DebugCondition.hh"
#include "MSXWatchIODevice.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MemBuffer.hh"
#include "StringOp.hh"
#include "KeyRange.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <cassert>
#include <stdexcept>

using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;
using std::begin;
using std::end;

namespace openmsx {

Debugger::Debugger(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, cmd(motherBoard.getCommandController(),
	      motherBoard.getStateChangeDistributor(),
	      motherBoard.getScheduler())
	, cpu(nullptr)
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

void Debugger::unregisterDebuggable(string_ref name, Debuggable& debuggable)
{
	assert(debuggables.contains(name));
	assert(debuggables[name.str()] == &debuggable); (void)debuggable;
	debuggables.erase(name);
}

Debuggable* Debugger::findDebuggable(string_ref name)
{
	auto it = debuggables.find(name);
	return (it != end(debuggables)) ? it->second : nullptr;
}

Debuggable& Debugger::getDebuggable(string_ref name)
{
	Debuggable* result = findDebuggable(name);
	if (!result) {
		throw CommandException("No such debuggable: " + name);
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

ProbeBase* Debugger::findProbe(string_ref name)
{
	auto it = probes.find(name);
	return (it != end(probes)) ? *it : nullptr;
}

ProbeBase& Debugger::getProbe(string_ref name)
{
	auto* result = findProbe(name);
	if (!result) {
		throw CommandException("No such probe: " + name);
	}
	return *result;
}

unsigned Debugger::insertProbeBreakPoint(
	TclObject command, TclObject condition,
	ProbeBase& probe, unsigned newId /*= -1*/)
{
	auto bp = make_unique<ProbeBreakPoint>(
		command, condition, *this, probe, newId);
	unsigned result = bp->getId();
	probeBreakPoints.push_back(std::move(bp));
	return result;
}

void Debugger::removeProbeBreakPoint(string_ref name)
{
	if (name.starts_with("pp#")) {
		// remove by id
		try {
			unsigned id = fast_stou(name.substr(3));
			auto it = find_if(begin(probeBreakPoints), end(probeBreakPoints),
				[&](std::unique_ptr<ProbeBreakPoint>& e)
					{ return e->getId() == id; });
			if (it == end(probeBreakPoints)) {
				throw CommandException("No such breakpoint: " + name);
			}
			move_pop_back(probeBreakPoints, it);
		} catch (std::invalid_argument&) {
			// parse error in fast_stou()
			throw CommandException("No such breakpoint: " + name);
		}
	} else {
		// remove by probe, only works for unconditional bp
		auto it = find_if(begin(probeBreakPoints), end(probeBreakPoints),
			[&](std::unique_ptr<ProbeBreakPoint>& e)
				{ return e->getProbe().getName() == name; });
		if (it == end(probeBreakPoints)) {
			throw CommandException(
				"No (unconditional) breakpoint for probe: " + name);
		}
		move_pop_back(probeBreakPoints, it);
	}
}

void Debugger::removeProbeBreakPoint(ProbeBreakPoint& bp)
{
	move_pop_back(probeBreakPoints, rfind_if_unguarded(probeBreakPoints,
		[&](ProbeBreakPoints::value_type& v) { return v.get() == &bp; }));
}

unsigned Debugger::setWatchPoint(TclObject command, TclObject condition,
                                 WatchPoint::Type type,
                                 unsigned beginAddr, unsigned endAddr,
                                 unsigned newId /*= -1*/)
{
	shared_ptr<WatchPoint> wp;
	if ((type == WatchPoint::READ_IO) || (type == WatchPoint::WRITE_IO)) {
		wp = make_shared<WatchIO>(
			motherBoard, type, beginAddr, endAddr,
			command, condition, newId);
	} else {
		wp = make_shared<WatchPoint>(
			command, condition, type, beginAddr, endAddr, newId);
	}
	motherBoard.getCPUInterface().setWatchPoint(wp);
	return wp->getId();
}

void Debugger::transfer(Debugger& other)
{
	// Copy watchpoints to new machine.
	assert(motherBoard.getCPUInterface().getWatchPoints().empty());
	for (auto& wp : other.motherBoard.getCPUInterface().getWatchPoints()) {
		setWatchPoint(wp->getCommandObj(), wp->getConditionObj(),
		              wp->getType(),       wp->getBeginAddress(),
		              wp->getEndAddress(), wp->getId());
	}

	// Copy probes to new machine.
	assert(probeBreakPoints.empty());
	for (auto& bp : other.probeBreakPoints) {
		if (ProbeBase* probe = findProbe(bp->getProbe().getName())) {
			insertProbeBreakPoint(bp->getCommandObj(),
			                      bp->getConditionObj(),
			                      *probe, bp->getId());
		}
	}

	// Breakpoints and conditions are (currently) global, so no need to
	// copy those.
}


// class Debugger::Cmd

static word getAddress(Interpreter& interp, array_ref<TclObject> tokens)
{
	if (tokens.size() < 3) {
		throw CommandException("Missing argument");
	}
	unsigned addr = tokens[2].getInt(interp);
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

bool Debugger::Cmd::needRecord(array_ref<TclObject> tokens) const
{
	// Note: it's crucial for security that only the write and write_block
	// subcommands are recorded and replayed. The 'set_bp' command for
	// example would allow to set a callback that can execute arbitrary Tcl
	// code. See comments in RecordedCommand for more details.
	if (tokens.size() < 2) return false;
	string_ref subCmd = tokens[1].getString();
	return (subCmd == "write") || (subCmd == "write_block");
}

void Debugger::Cmd::execute(
	array_ref<TclObject> tokens, TclObject& result, EmuTime::param /*time*/)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	string_ref subCmd = tokens[1].getString();
	if (subCmd == "read") {
		read(tokens, result);
	} else if (subCmd == "read_block") {
		readBlock(tokens, result);
	} else if (subCmd == "write") {
		write(tokens, result);
	} else if (subCmd == "write_block") {
		writeBlock(tokens, result);
	} else if (subCmd == "size") {
		size(tokens, result);
	} else if (subCmd == "desc") {
		desc(tokens, result);
	} else if (subCmd == "list") {
		list(result);
	} else if (subCmd == "step") {
		debugger().motherBoard.getCPUInterface().doStep();
	} else if (subCmd == "cont") {
		debugger().motherBoard.getCPUInterface().doContinue();
	} else if (subCmd == "disasm") {
		debugger().cpu->disasmCommand(getInterpreter(), tokens, result);
	} else if (subCmd == "break") {
		debugger().motherBoard.getCPUInterface().doBreak();
	} else if (subCmd == "breaked") {
		result.setInt(debugger().motherBoard.getCPUInterface().isBreaked());
	} else if (subCmd == "set_bp") {
		setBreakPoint(tokens, result);
	} else if (subCmd == "remove_bp") {
		removeBreakPoint(tokens, result);
	} else if (subCmd == "list_bp") {
		listBreakPoints(tokens, result);
	} else if (subCmd == "set_watchpoint") {
		setWatchPoint(tokens, result);
	} else if (subCmd == "remove_watchpoint") {
		removeWatchPoint(tokens, result);
	} else if (subCmd == "list_watchpoints") {
		listWatchPoints(tokens, result);
	} else if (subCmd == "set_condition") {
		setCondition(tokens, result);
	} else if (subCmd == "remove_condition") {
		removeCondition(tokens, result);
	} else if (subCmd == "list_conditions") {
		listConditions(tokens, result);
	} else if (subCmd == "probe") {
		probe(tokens, result);
	} else {
		throw SyntaxError();
	}
}

void Debugger::Cmd::list(TclObject& result)
{
	result.addListElements(keys(debugger().debuggables));
}

void Debugger::Cmd::desc(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	result.setString(device.getDescription());
}

void Debugger::Cmd::size(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	result.setInt(device.getSize());
}

void Debugger::Cmd::read(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned addr = tokens[3].getInt(getInterpreter());
	if (addr >= device.getSize()) {
		throw CommandException("Invalid address");
	}
	result.setInt(device.read(addr));
}

void Debugger::Cmd::readBlock(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
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
	for (unsigned i = 0; i < num; ++i) {
		buf[i] = device.read(addr + i);
	}
	result.setBinary(buf.data(), num);
}

void Debugger::Cmd::write(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
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

void Debugger::Cmd::writeBlock(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable& device = debugger().getDebuggable(tokens[2].getString());
	unsigned devSize = device.getSize();
	unsigned addr = tokens[3].getInt(getInterpreter());
	if (addr >= devSize) {
		throw CommandException("Invalid address");
	}
	unsigned num;
	const byte* buf = tokens[4].getBinary(num);
	if ((num + addr) > devSize) {
		throw CommandException("Invalid size");
	}

	for (unsigned i = 0; i < num; ++i) {
		device.write(addr + i, static_cast<byte>(buf[i]));
	}
}

void Debugger::Cmd::setBreakPoint(array_ref<TclObject> tokens, TclObject& result)
{
	TclObject command("debug break");
	TclObject condition;

	switch (tokens.size()) {
	case 5: // command
		command = tokens[4];
		// fall-through
	case 4: // condition
		condition = tokens[3];
		// fall-through
	case 3: { // address
		word addr = getAddress(getInterpreter(), tokens);
		BreakPoint bp(addr, command, condition);
		result.setString(StringOp::Builder() << "bp#" << bp.getId());
		debugger().motherBoard.getCPUInterface().insertBreakPoint(bp);
		break;
	}
	default:
		if (tokens.size() < 3) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}
}

void Debugger::Cmd::removeBreakPoint(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto& interface = debugger().motherBoard.getCPUInterface();
	auto& breakPoints = interface.getBreakPoints();

	string_ref tmp = tokens[2].getString();
	if (tmp.starts_with("bp#")) {
		// remove by id
		try {
			unsigned id = fast_stou(tmp.substr(3));
			auto it = find_if(begin(breakPoints), end(breakPoints),
				[&](const BreakPoint& bp) { return bp.getId() == id; });
			if (it == end(breakPoints)) {
				throw CommandException("No such breakpoint: " + tmp);
			}
			interface.removeBreakPoint(*it);
		} catch (std::invalid_argument&) {
			// parse error in fast_stou()
			throw CommandException("No such breakpoint: " + tmp);
		}
	} else {
		// remove by addr, only works for unconditional bp
		word addr = getAddress(getInterpreter(), tokens);
		auto range = equal_range(begin(breakPoints), end(breakPoints),
		                         addr, CompareBreakpoints());
		auto it = find_if(range.first, range.second,
			[&](const BreakPoint& bp) {
				return bp.getCondition().empty(); });
		if (it == range.second) {
			throw CommandException(
				"No (unconditional) breakpoint at address: " + tmp);
		}
		interface.removeBreakPoint(*it);
	}
}

void Debugger::Cmd::listBreakPoints(
	array_ref<TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& bp : interface.getBreakPoints()) {
		TclObject line;
		line.addListElement(StringOp::Builder() << "bp#" << bp.getId());
		line.addListElement("0x" + StringOp::toHexString(bp.getAddress(), 4));
		line.addListElement(bp.getCondition());
		line.addListElement(bp.getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}


void Debugger::Cmd::setWatchPoint(array_ref<TclObject> tokens, TclObject& result)
{
	TclObject command("debug break");
	TclObject condition;
	unsigned beginAddr, endAddr;
	WatchPoint::Type type;

	switch (tokens.size()) {
	case 6: // command
		command = tokens[5];
		// fall-through
	case 5: // condition
		condition = tokens[4];
		// fall-through
	case 4: { // address + type
		string_ref typeStr = tokens[2].getString();
		unsigned max;
		if (typeStr == "read_io") {
			type = WatchPoint::READ_IO;
			max = 0x100;
		} else if (typeStr == "write_io") {
			type = WatchPoint::WRITE_IO;
			max = 0x100;
		} else if (typeStr == "read_mem") {
			type = WatchPoint::READ_MEM;
			max = 0x10000;
		} else if (typeStr == "write_mem") {
			type = WatchPoint::WRITE_MEM;
			max = 0x10000;
		} else {
			throw CommandException("Invalid type: " + typeStr);
		}
		auto& interp = getInterpreter();
		if (tokens[3].getListLength(interp) == 2) {
			beginAddr = tokens[3].getListIndex(interp, 0).getInt(interp);
			endAddr   = tokens[3].getListIndex(interp, 1).getInt(interp);
			if (endAddr < beginAddr) {
				throw CommandException(
					"Not a valid range: end address may "
					"not be smaller than begin address.");
			}
		} else {
			beginAddr = endAddr = tokens[3].getInt(interp);
		}
		if (endAddr >= max) {
			throw CommandException("Invalid address: out of range");
		}
		break;
	}
	default:
		if (tokens.size() < 4) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}
	unsigned id = debugger().setWatchPoint(
		command, condition, type, beginAddr, endAddr);
	result.setString(StringOp::Builder() << "wp#" << id);
}

void Debugger::Cmd::removeWatchPoint(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	string_ref tmp = tokens[2].getString();
	try {
		if (tmp.starts_with("wp#")) {
			// remove by id
			unsigned id = fast_stou(tmp.substr(3));
			auto& interface = debugger().motherBoard.getCPUInterface();
			for (auto& wp : interface.getWatchPoints()) {
				if (wp->getId() == id) {
					interface.removeWatchPoint(wp);
					return;
				}
			}
		}
	} catch (std::invalid_argument&) {
		// parse error in fast_stou()
	}
	throw CommandException("No such watchpoint: " + tmp);
}

void Debugger::Cmd::listWatchPoints(
	array_ref<TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& wp : interface.getWatchPoints()) {
		TclObject line;
		line.addListElement(StringOp::Builder() << "wp#" << wp->getId());
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
			line.addListElement("0x" + StringOp::toHexString(beginAddr, 4));
		} else {
			TclObject range;
			range.addListElement("0x" + StringOp::toHexString(beginAddr, 4));
			range.addListElement("0x" + StringOp::toHexString(endAddr,   4));
			line.addListElement(range);
		}
		line.addListElement(wp->getCondition());
		line.addListElement(wp->getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}


void Debugger::Cmd::setCondition(array_ref<TclObject> tokens, TclObject& result)
{
	TclObject command("debug break");
	TclObject condition;

	switch (tokens.size()) {
	case 4: // command
		command = tokens[3];
		// fall-through
	case 3: { // condition
		condition = tokens[2];
		DebugCondition dc(command, condition);
		result.setString(StringOp::Builder() << "cond#" << dc.getId());
		debugger().motherBoard.getCPUInterface().setCondition(dc);
		break;
	}
	default:
		if (tokens.size() < 3) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}
}

void Debugger::Cmd::removeCondition(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}

	string_ref tmp = tokens[2].getString();
	try {
		if (tmp.starts_with("cond#")) {
			// remove by id
			unsigned id = fast_stou(tmp.substr(5));
			auto& interface = debugger().motherBoard.getCPUInterface();
			for (auto& c : interface.getConditions()) {
				if (c.getId() == id) {
					interface.removeCondition(c);
					return;
				}
			}
		}
	} catch (std::invalid_argument&) {
		// parse error in fast_stou()
	}
	throw CommandException("No such condition: " + tmp);
}

void Debugger::Cmd::listConditions(
	array_ref<TclObject> /*tokens*/, TclObject& result)
{
	string res;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& c : interface.getConditions()) {
		TclObject line;
		line.addListElement(StringOp::Builder() << "cond#" << c.getId());
		line.addListElement(c.getCondition());
		line.addListElement(c.getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}


void Debugger::Cmd::probe(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 3) {
		throw CommandException("Missing argument");
	}
	string_ref subCmd = tokens[2].getString();
	if (subCmd == "list") {
		probeList(tokens, result);
	} else if (subCmd == "desc") {
		probeDesc(tokens, result);
	} else if (subCmd == "read") {
		probeRead(tokens, result);
	} else if (subCmd == "set_bp") {
		probeSetBreakPoint(tokens, result);
	} else if (subCmd == "remove_bp") {
		probeRemoveBreakPoint(tokens, result);
	} else if (subCmd == "list_bp") {
		probeListBreakPoints(tokens, result);
	} else {
		throw SyntaxError();
	}
}
void Debugger::Cmd::probeList(array_ref<TclObject> /*tokens*/, TclObject& result)
{
	// TODO use transform_iterator or transform_view
	for (auto* p : debugger().probes) {
		result.addListElement(p->getName());
	}
}
void Debugger::Cmd::probeDesc(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	result.setString(debugger().getProbe(tokens[3].getString()).getDescription());
}
void Debugger::Cmd::probeRead(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	result.setString(debugger().getProbe(tokens[3].getString()).getValue());
}
void Debugger::Cmd::probeSetBreakPoint(
	array_ref<TclObject> tokens, TclObject& result)
{
	TclObject command("debug break");
	TclObject condition;
	ProbeBase* p;

	switch (tokens.size()) {
	case 6: // command
		command = tokens[5];
		// fall-through
	case 5: // condition
		condition = tokens[4];
		// fall-through
	case 4: { // probe
		p = &debugger().getProbe(tokens[3].getString());
		break;
	}
	default:
		if (tokens.size() < 4) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}

	unsigned id = debugger().insertProbeBreakPoint(command, condition, *p);
	result.setString(StringOp::Builder() << "pp#" << id);
}
void Debugger::Cmd::probeRemoveBreakPoint(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	debugger().removeProbeBreakPoint(tokens[3].getString());
}
void Debugger::Cmd::probeListBreakPoints(
	array_ref<TclObject> /*tokens*/, TclObject& result)
{
	string res;
	for (auto& p : debugger().probeBreakPoints) {
		TclObject line;
		line.addListElement(StringOp::Builder() << "pp#" << p->getId());
		line.addListElement(p->getProbe().getName());
		line.addListElement(p->getCondition());
		line.addListElement(p->getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}

string Debugger::Cmd::help(const vector<string>& tokens) const
{
	static const string generalHelp =
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

	static const string listHelp =
		"debug list\n"
		"  Returns a list with the names of all 'debuggables'.\n"
		"  These names are used in other debug subcommands.\n";
	static const string descHelp =
		"debug desc <name>\n"
		"  Returns a description for the debuggable with given name.\n";
	static const string sizeHelp =
		"debug size <name>\n"
		"  Returns the size (in bytes) of the debuggable with given name.\n";
	static const string readHelp =
		"debug read <name> <addr>\n"
		"  Read a byte at offset <addr> from the given debuggable.\n"
		"  The offset must be smaller than the value returned from the "
		"'size' subcommand\n"
		"  Note that openMSX comes with a bunch of Tcl scripts that make "
		"some of the debug reads much more convenient (e.g. reading from "
		"Z80 or VDP registers). See the Console Command Reference for more "
		"details about these.\n";
	static const string writeHelp =
		"debug write <name> <addr> <val>\n"
		"  Write a byte to the given debuggable at a certain offset.\n"
		"  The offset must be smaller than the value returned from the "
		"'size' subcommand\n";
	static const string readBlockHelp =
		"debug read_block <name> <addr> <size>\n"
		"  Read a whole block at once. This is equivalent with repeated "
		"invocations of the 'read' subcommand, but using this subcommand "
		"may be faster. The result is a Tcl binary string (see Tcl manual).\n"
		"  The block is specified as size/offset in the debuggable. The "
		"complete block must fit in the debuggable (see the 'size' "
		"subcommand).\n";
	static const string writeBlockHelp =
		"debug write_block <name> <addr> <values>\n"
		"  Write a whole block at once. This is equivalent with repeated "
		"invocations of the 'write' subcommand, but using this subcommand "
		"may be faster. The <values> argument must be a Tcl binary string "
		"(see Tcl manual).\n"
		"  The block has a size and an offset in the debuggable. The "
		"complete block must fit in the debuggable (see the 'size' "
		"subcommand).\n";
	static const string setBpHelp =
		"debug set_bp <addr> [<cond>] [<cmd>]\n"
		"  Insert a new breakpoint at given address. When the CPU is about "
		"to execute the instruction at this address, execution will be "
		"breaked. At least this is the default behaviour, see next "
		"paragraphs.\n"
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
	static const string removeBpHelp =
		"debug remove_bp <id>\n"
		"  Remove the breakpoint with given ID again. You can use the "
		"'list_bp' subcommand to see all valid IDs.\n";
	static const string listBpHelp =
		"debug list_bp\n"
		"  Lists all active breakpoints. The result is printed in 4 "
		"columns. The first column contains the breakpoint ID. The "
		"second one has the address. The third has the condition "
		"(default condition is empty). And the last column contains "
		"the command that will be executed (default is 'debug break').\n";
	static const string setWatchPointHelp =
		"debug set_watchpoint <type> <region> [<cond>] [<cmd>]\n"
		"  Insert a new watchpoint of given type on the given region, "
		"there can be an optional condition and alternative command. See "
		"the 'set_bp' subcommand for details about these last two.\n"
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
	static const string removeWatchPointHelp =
		"debug remove_watchpoint <id>\n"
		"  Remove the watchpoint with given ID again. You can use the "
		"'list_watchpoints' subcommand to see all valid IDs.\n";
	static const string listWatchPointsHelp =
		"debug list_watchpoints\n"
		"  Lists all active watchpoints. The result is similar to the "
		"'list_bp' subcommand, but there is an extra column (2nd column) "
		"that contains the type of the watchpoint.\n";
	static const string setCondHelp =
		"debug set_condition <cond> [<cmd>]\n"
		"  Insert a new condition. These are much like breakpoints, "
		"except that they are checked before every instruction "
		"(breakpoints are tied to a specific address).\n"
		"  Conditions will slow down simulation MUCH more than "
		"breakpoints. So only use them when you don't care about "
		"simulation speed (when you're debugging this is usually not "
		"a problem).\n"
		"  See 'help debug set_bp' for more details.\n";
	static const string removeCondHelp =
		"debug remove_condition <id>\n"
		"  Remove the condition with given ID again. You can use the "
		"'list_conditions' subcommand to see all valid IDs.\n";
	static const string listCondHelp =
		"debug list_conditions\n"
		"  Lists all active conditions. The result is similar to the "
		"'list_bp' subcommand, but without the 2nd column that would "
		"show the address.\n";
	static const string probeHelp =
		"debug probe <subcommand> [<arguments>]\n"
		"  Possible subcommands are:\n"
		"    list                             returns a list of all probes\n"
		"    desc   <probe>                   returns a description of this probe\n"
		"    read   <probe>                   returns the current value of this probe\n"
		"    set_bp <probe> [<cond>] [<cmd>]  set a breakpoint on the given probe\n"
		"    remove_bp <id>                   remove the given breakpoint\n"
		"    list_bp                          returns a list of breakpoints that are set on probes\n";
	static const string contHelp =
		"debug cont\n"
		"  Continue execution after CPU was breaked.\n";
	static const string stepHelp =
		"debug step\n"
		"  Execute one instruction. This command is only meaningful in "
		"break mode.\n";
	static const string breakHelp =
		"debug break\n"
		"  Immediately break CPU execution. When CPU was already breaked "
		"this command has no effect.\n";
	static const string breakedHelp =
		"debug breaked\n"
		"  Query the CPU breaked status. Returns '1' when CPU was "
		"breaked, '0' otherwise.\n";
	static const string disasmHelp =
		"debug disasm <addr>\n"
		"  Disassemble the instruction at the given address. The result "
		"is a Tcl list. The first element in the list contains a textual "
		"representation of the instruction, the next elements contain the "
		"bytes that make up this instruction (thus the length of the "
		"resulting list can be used to derive the number of bytes in the "
		"instruction).\n"
		"  Note that openMSX comes with a 'disasm' Tcl script that is much "
		"more convenient to use than this subcommand.";
	static const string unknownHelp =
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

vector<string> Debugger::Cmd::getBreakPointIds() const
{
	vector<string> bpids;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& bp : interface.getBreakPoints()) {
		bpids.push_back(StringOp::Builder() << "bp#" << bp.getId());
	}
	return bpids;
}
vector<string> Debugger::Cmd::getWatchPointIds() const
{
	vector<string> wpids;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& w : interface.getWatchPoints()) {
		wpids.push_back(StringOp::Builder() << "wp#" << w->getId());
	}
	return wpids;
}
vector<string> Debugger::Cmd::getConditionIds() const
{
	vector<string> condids;
	auto& interface = debugger().motherBoard.getCPUInterface();
	for (auto& c : interface.getConditions()) {
		condids.push_back(StringOp::Builder() << "cond#" << c.getId());
	}
	return condids;
}

void Debugger::Cmd::tabCompletion(vector<string>& tokens) const
{
	static const char* const singleArgCmds[] = {
		"list", "step", "cont", "break", "breaked",
		"list_bp", "list_watchpoints", "list_conditions",
	};
	static const char* const debuggableArgCmds[] = {
		"desc", "size", "read", "read_block",
		"write", "write_block",
	};
	static const char* const otherCmds[] = {
		"disasm", "set_bp", "remove_bp", "set_watchpoint",
		"remove_watchpoint", "set_condition", "remove_condition",
		"probe",
	};
	switch (tokens.size()) {
	case 2: {
		vector<const char*> cmds;
		cmds.insert(end(cmds), begin(singleArgCmds),     end(singleArgCmds));
		cmds.insert(end(cmds), begin(debuggableArgCmds), end(debuggableArgCmds));
		cmds.insert(end(cmds), begin(otherCmds),         end(otherCmds));
		completeString(tokens, cmds);
		break;
	}
	case 3:
		if (!contains(singleArgCmds, tokens[1])) {
			// this command takes (an) argument(s)
			if (contains(debuggableArgCmds, tokens[1])) {
				// it takes a debuggable here
				completeString(tokens, keys(debugger().debuggables));
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
				static const char* const types[] = {
					"write_io", "write_mem",
					"read_io", "read_mem",
				};
				completeString(tokens, types);
			} else if (tokens[1] == "probe") {
				static const char* const subCmds[] = {
					"list", "desc", "read", "set_bp",
					"remove_bp", "list_bp",
				};
				completeString(tokens, subCmds);
			}
		}
		break;
	case 4:
		if ((tokens[1] == "probe") &&
		    ((tokens[2] == "desc") || (tokens[2] == "read") ||
		     (tokens[2] == "set_bp"))) {
			std::vector<string_ref> probeNames;
			for (auto* p : debugger().probes) {
				probeNames.push_back(p->getName());
			}
			completeString(tokens, probeNames);
		}
		break;
	}
}

} // namespace openmsx
