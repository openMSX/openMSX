// $Id$

#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "BreakPoint.hh"
#include "MSXWatchIODevice.hh"
#include "TclObject.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include <cassert>
#include <sstream>
#include <iomanip>
#include <memory>

using std::map;
using std::ostringstream;
using std::set;
using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class DebugCmd : public Command
{
public:
	DebugCmd(CommandController& commandController,
		 Debugger& debugger);
	virtual void execute(const std::vector<TclObject*>& tokens,
			     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	void list(TclObject& result);
	void desc(const std::vector<TclObject*>& tokens,
	          TclObject& result);
	void size(const std::vector<TclObject*>& tokens,
	          TclObject& result);
	void read(const std::vector<TclObject*>& tokens,
	          TclObject& result);
	void readBlock(const std::vector<TclObject*>& tokens,
	               TclObject& result);
	void write(const std::vector<TclObject*>& tokens,
	           TclObject& result);
	void writeBlock(const std::vector<TclObject*>& tokens,
	                TclObject& result);
	void setBreakPoint(const std::vector<TclObject*>& tokens,
	                   TclObject& result);
	void removeBreakPoint(const std::vector<TclObject*>& tokens,
	                      TclObject& result);
	void listBreakPoints(const std::vector<TclObject*>& tokens,
	                     TclObject& result);
	std::set<std::string> getBreakPointIdsAsStringSet() const;
	void setWatchPoint(const std::vector<TclObject*>& tokens,
	                   TclObject& result);
	void removeWatchPoint(const std::vector<TclObject*>& tokens,
	                      TclObject& result);
	void listWatchPoints(const std::vector<TclObject*>& tokens,
	                     TclObject& result);

	Debugger& debugger;
};


Debugger::Debugger(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, debugCmd(new DebugCmd(motherBoard.getCommandController(), *this))
	, cpu(0)
{
}

Debugger::~Debugger()
{
	assert(!cpu);
	assert(debuggables.empty());
}

void Debugger::setCPU(MSXCPU* cpu_)
{
	cpu = cpu_;
}

void Debugger::registerDebuggable(const string& name, Debuggable& debuggable)
{
	assert(debuggables.find(name) == debuggables.end());
	debuggables[name] = &debuggable;
}

void Debugger::unregisterDebuggable(const string& name, Debuggable& debuggable)
{
	(void)debuggable;
	map<string, Debuggable*>::iterator it = debuggables.find(name);
	assert(it != debuggables.end() && (it->second == &debuggable));
	debuggables.erase(it);
}

Debuggable* Debugger::findDebuggable(const string& name)
{
	map<string, Debuggable*>::iterator it = debuggables.find(name);
	return (it != debuggables.end()) ? it->second : NULL;
}

Debuggable* Debugger::getDebuggable(const string& name)
{
	Debuggable* result = findDebuggable(name);
	if (!result) {
		throw CommandException("No such debuggable.");
	}
	return result;
}

void Debugger::getDebuggables(set<string>& result) const
{
	for (map<string, Debuggable*>::const_iterator it = debuggables.begin();
	     it != debuggables.end(); ++it) {
		result.insert(it->first);
	}
}


// class DebugCmd

static word getAddress(const vector<TclObject*>& tokens)
{
	if (tokens.size() < 3) {
		throw CommandException("Missing argument");
	}
	unsigned addr = tokens[2]->getInt();
	if (addr >= 0x10000) {
		throw CommandException("Invalid address");
	}
	return addr;
}

DebugCmd::DebugCmd(CommandController& commandController_,
                             Debugger& debugger_)
	: Command(commandController_, "debug")
	, debugger(debugger_)
{
}

void DebugCmd::execute(const vector<TclObject*>& tokens,
                                 TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	string subCmd = tokens[1]->getString();
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
		debugger.cpu->doStep();
	} else if (subCmd == "cont") {
		debugger.cpu->doContinue();
	} else if (subCmd == "disasm") {
		debugger.cpu->disasmCommand(tokens, result);
	} else if (subCmd == "break") {
		debugger.cpu->doBreak();
	} else if (subCmd == "breaked") {
		result.setInt(debugger.cpu->isBreaked());
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
	} else {
		throw SyntaxError();
	}
}

void DebugCmd::list(TclObject& result)
{
	for (map<string, Debuggable*>::const_iterator it =
	       debugger.debuggables.begin();
	     it != debugger.debuggables.end(); ++it) {
		result.addListElement(it->first);
	}
}

void DebugCmd::desc(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	result.setString(device->getDescription());
}

void DebugCmd::size(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	result.setInt(device->getSize());
}

void DebugCmd::read(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	unsigned addr = tokens[3]->getInt();
	if (addr >= device->getSize()) {
		throw CommandException("Invalid address");
	}
	result.setInt(device->read(addr));
}

void DebugCmd::readBlock(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	unsigned size = device->getSize();
	unsigned addr = tokens[3]->getInt();
	if (addr >= size) {
		throw CommandException("Invalid address");
	}
	unsigned num = tokens[4]->getInt();
	if (num > (size - addr)) {
		throw CommandException("Invalid size");
	}

	byte* buf = new byte[num];
	for (unsigned i = 0; i < num; ++i) {
		buf[i] = device->read(addr + i);
	}
	result.setBinary(buf, num);
	delete[] buf;
}

void DebugCmd::write(const vector<TclObject*>& tokens,
                               TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	unsigned addr = tokens[3]->getInt();
	if (addr >= device->getSize()) {
		throw CommandException("Invalid address");
	}
	unsigned value = tokens[4]->getInt();
	if (value >= 256) {
		throw CommandException("Invalid value");
	}

	device->write(addr, value);
}

void DebugCmd::writeBlock(const vector<TclObject*>& tokens,
                                    TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = debugger.getDebuggable(tokens[2]->getString());
	unsigned size = device->getSize();
	unsigned addr = tokens[3]->getInt();
	if (addr >= size) {
		throw CommandException("Invalid address");
	}
	unsigned num;
	const byte* buf = tokens[4]->getBinary(num);
	if ((num + addr) > size) {
		throw CommandException("Invalid size");
	}

	for (unsigned i = 0; i < num; ++i) {
		device->write(addr + i, static_cast<byte>(buf[i]));
	}
}

void DebugCmd::setBreakPoint(const vector<TclObject*>& tokens,
                             TclObject& result)
{
	auto_ptr<BreakPoint> bp;
	word addr;
	auto_ptr<TclObject> command(
		new TclObject(result.getInterpreter(), "debug break"));
	auto_ptr<TclObject> condition;
	switch (tokens.size()) {
	case 5: // command
		command->setString(tokens[4]->getString());
		command->checkCommand();
		// fall-through
	case 4: // condition
		if (!tokens[3]->getString().empty()) {
			condition.reset(new TclObject(*tokens[3]));
			condition->checkExpression();
		}
		// fall-through
	case 3: // address
		addr = getAddress(tokens);
		bp.reset(new BreakPoint(getCommandController().getCliComm(),
		                        addr, command, condition));
		break;
	default:
		if (tokens.size() < 3) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}
	result.setString("bp#" + StringOp::toString(bp->getId()));
	debugger.cpu->insertBreakPoint(bp);
}

void DebugCmd::removeBreakPoint(const std::vector<TclObject*>& tokens,
                                          TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	const CPU::BreakPoints& breakPoints = debugger.cpu->getBreakPoints();

	string tmp = tokens[2]->getString();
	if (StringOp::startsWith(tmp, "bp#")) {
		// remove by id
		unsigned id = StringOp::stringToInt(tmp.substr(3));
		for (CPU::BreakPoints::const_iterator it = breakPoints.begin();
		     it != breakPoints.end(); ++it) {
			const BreakPoint& bp = *it->second;
			if (bp.getId() == id) {
				debugger.cpu->removeBreakPoint(bp);
				return;
			}
		}
		throw CommandException("No such breakpoint: " + tmp);
	} else {
		// remove by addr, only works for unconditional bp
		word addr = getAddress(tokens);
		std::pair<CPU::BreakPoints::const_iterator,
			  CPU::BreakPoints::const_iterator> range =
				breakPoints.equal_range(addr);
		for (CPU::BreakPoints::const_iterator it = range.first;
		     it != range.second; ++it) {
			const BreakPoint& bp = *it->second;
			if (bp.getCondition().empty()) {
				debugger.cpu->removeBreakPoint(bp);
				return;
			}
		}
		throw CommandException(
			"No (unconditional) breakpoint at address: " + tmp);
	}
}

void DebugCmd::listBreakPoints(const std::vector<TclObject*>& /*tokens*/,
                               TclObject& result)
{
	const CPU::BreakPoints& breakPoints = debugger.cpu->getBreakPoints();
	string res;
	for (CPU::BreakPoints::const_iterator it = breakPoints.begin();
	     it != breakPoints.end(); ++it) {
		const BreakPoint& bp = *it->second;
		TclObject line(result.getInterpreter());
		line.addListElement("bp#" + StringOp::toString(bp.getId()));
		line.addListElement("0x" + StringOp::toHexString(bp.getAddress(), 4));
		line.addListElement(bp.getCondition());
		line.addListElement(bp.getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}

void DebugCmd::setWatchPoint(const std::vector<TclObject*>& tokens,
                             TclObject& result)
{
	auto_ptr<WatchPoint> wp;
	auto_ptr<TclObject> command(
		new TclObject(result.getInterpreter(), "debug break"));
	auto_ptr<TclObject> condition;

	switch (tokens.size()) {
	case 6: // command
		command->setString(tokens[5]->getString());
		command->checkCommand();
		// fall-through
	case 5: // condition
		if (!tokens[4]->getString().empty()) {
			condition.reset(new TclObject(*tokens[4]));
			condition->checkExpression();
		}
		// fall-through
	case 4: { // address + type
		string typeStr = tokens[2]->getString();
		WatchPoint::Type type;
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
		unsigned addr = tokens[3]->getInt();
		if (addr >= max) {
			throw CommandException("Invalid address");
		}
		wp.reset(new MSXWatchIODevice(debugger.motherBoard, type, addr,
		                              command, condition));
		break;
	}
	default:
		if (tokens.size() < 4) {
			throw CommandException("Too few arguments.");
		} else {
			throw CommandException("Too many arguments.");
		}
	}
	result.setString("wp#" + StringOp::toString(wp->getId()));
	debugger.motherBoard.getCPUInterface().setWatchPoint(wp);
}

void DebugCmd::removeWatchPoint(const std::vector<TclObject*>& tokens,
                                TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	MSXCPUInterface& interface = debugger.motherBoard.getCPUInterface();
	const MSXCPUInterface::WatchPoints& watchPoints = interface.getWatchPoints();

	string tmp = tokens[2]->getString();
	if (StringOp::startsWith(tmp, "wp#")) {
		// remove by id
		unsigned id = StringOp::stringToInt(tmp.substr(3));
		for (MSXCPUInterface::WatchPoints::const_iterator it =
			watchPoints.begin(); it != watchPoints.end(); ++it) {
			WatchPoint& wp = **it;
			if (wp.getId() == id) {
				interface.removeWatchPoint(wp);
				return;
			}
		}
	}
	throw CommandException("No such watchpoint: " + tmp);
}

void DebugCmd::listWatchPoints(const std::vector<TclObject*>& /*tokens*/,
                               TclObject& result)
{
	MSXCPUInterface& interface = debugger.motherBoard.getCPUInterface();
	const MSXCPUInterface::WatchPoints& watchPoints = interface.getWatchPoints();
	string res;
	for (MSXCPUInterface::WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		const WatchPoint& wp = **it;
		TclObject line(result.getInterpreter());
		line.addListElement("wp#" + StringOp::toString(wp.getId()));
		string type;
		switch (wp.getType()) {
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
			assert(false);
			break;
		}
		line.addListElement(type);
		line.addListElement("0x" + StringOp::toHexString(wp.getAddress(), 4));
		line.addListElement(wp.getCondition());
		line.addListElement(wp.getCommand());
		res += line.getString() + '\n';
	}
	result.setString(res);
}

string DebugCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText =
		"debug list                                returns a list of all debuggables\n"
		"debug desc <name>                         returns a description of this debuggable\n"
		"debug size <name>                         returns the size of this debuggable\n"
		"debug read <name> <addr>                  read a byte from a debuggable\n"
		"debug write <name> <addr> <val>           write a byte to a debuggable\n"
		"debug read_block <name> <addr> <size>     read a whole block at once\n"
		"debug write_block <name> <addr> <values>  write a whole block at once\n"
		"debug set_bp <addr> [<cond>] [<cmd>]      insert a new breakpoint\n"
		"debug remove_bp <id>                      remove a certain breapoint\n"
		"debug list_bp                             list the active breakpoints\n"
		"debug cont                                continue execution after break\n"
		"debug step                                execute one instruction\n"
		"debug break                               break CPU at current position\n"
		"debug breaked                             query CPU breaked status\n"
		"debug disasm <addr>                       disassemble instruction\n";
	return helpText;
}

set<string> DebugCmd::getBreakPointIdsAsStringSet() const
{
	const CPU::BreakPoints& breakPoints = debugger.cpu->getBreakPoints();
	set<string> bpids;
	for (CPU::BreakPoints::const_iterator it = breakPoints.begin();
	     it != breakPoints.end(); ++it) {
		bpids.insert("bp#" + StringOp::toString((*it->second).getId()));
	}
	return bpids;
}

void DebugCmd::tabCompletion(vector<string>& tokens) const
{
	set<string> singleArgCmds;
	singleArgCmds.insert("list");
	singleArgCmds.insert("step");
	singleArgCmds.insert("cont");
	singleArgCmds.insert("break");
	singleArgCmds.insert("breaked");
	singleArgCmds.insert("list_bp");
	singleArgCmds.insert("list_watchpoints");
	set<string> debuggableArgCmds;
	debuggableArgCmds.insert("desc");
	debuggableArgCmds.insert("size");
	debuggableArgCmds.insert("read");
	debuggableArgCmds.insert("read_block");
	debuggableArgCmds.insert("write");
	debuggableArgCmds.insert("write_block");
	set<string> otherCmds;
	otherCmds.insert("disasm");
	otherCmds.insert("set_bp");
	otherCmds.insert("remove_bp");
	otherCmds.insert("set_watchpoint");
	otherCmds.insert("remove_watchpoint");
	switch (tokens.size()) {
		case 2: {
			set<string> cmds;
			cmds.insert(singleArgCmds.begin(), singleArgCmds.end());
			cmds.insert(debuggableArgCmds.begin(), debuggableArgCmds.end());
			cmds.insert(otherCmds.begin(), otherCmds.end());
			completeString(tokens, cmds);
			break;
		}
		case 3: {
			if (singleArgCmds.find(tokens[1]) ==
					singleArgCmds.end()) {
				// this command takes (an) argument(s)
				if (debuggableArgCmds.find(tokens[1]) !=
					debuggableArgCmds.end()) {
					// it takes a debuggable here
					set<string> debuggables;
					debugger.getDebuggables(debuggables);
					completeString(tokens, debuggables);
				} else if (tokens[1] == "remove_bp") {
					// this one takes a bp id
					set<string> bpids = getBreakPointIdsAsStringSet();
					completeString(tokens, bpids);
				} else if (tokens[1] == "remove_watchpoint") {
					// this one takes a wp id
					// TODO
					//set<string> bpids = getBreakPointIdsAsStringSet();
					//completeString(tokens, bpids);
				}
			}
			break;
		// other stuff doesn't really make sense to complete
		}
	}
}

} // namespace openmsx
