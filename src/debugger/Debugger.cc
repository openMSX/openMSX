// $Id$

#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "BreakPoint.hh"
#include "MSXWatchIODevice.hh"
#include "TclObject.hh"
#include "MSXCommandController.hh"
#include "RecordedCommand.hh"
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

class DebugCmd : public RecordedCommand
{
public:
	DebugCmd(MSXCommandController& msxCommandController,
	         MSXEventDistributor& msxEventDistributor,
	         Scheduler& scheduler, MSXCliComm& cliComm,
	         Debugger& debugger);
	virtual bool needRecord(const vector<TclObject*>& tokens) const;
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result, const EmuTime& time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;

private:
	void list(TclObject& result);
	void desc(const vector<TclObject*>& tokens,
	          TclObject& result);
	void size(const vector<TclObject*>& tokens,
	          TclObject& result);
	void read(const vector<TclObject*>& tokens,
	          TclObject& result);
	void readBlock(const vector<TclObject*>& tokens,
	               TclObject& result);
	void write(const vector<TclObject*>& tokens,
	           TclObject& result);
	void writeBlock(const vector<TclObject*>& tokens,
	                TclObject& result);
	void setBreakPoint(const vector<TclObject*>& tokens,
	                   TclObject& result);
	void removeBreakPoint(const vector<TclObject*>& tokens,
	                      TclObject& result);
	void listBreakPoints(const vector<TclObject*>& tokens,
	                     TclObject& result);
	set<string> getBreakPointIdsAsStringSet() const;
	set<string> getWatchPointIdsAsStringSet() const;
	void setWatchPoint(const vector<TclObject*>& tokens,
	                   TclObject& result);
	void removeWatchPoint(const vector<TclObject*>& tokens,
	                      TclObject& result);
	void listWatchPoints(const vector<TclObject*>& tokens,
	                     TclObject& result);

	MSXCliComm& cliComm;
	Debugger& debugger;
};


Debugger::Debugger(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, debugCmd(new DebugCmd(motherBoard.getMSXCommandController(),
	                        motherBoard.getMSXEventDistributor(),
	                        motherBoard.getScheduler(),
	                        motherBoard.getMSXCliComm(), *this))
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

DebugCmd::DebugCmd(MSXCommandController& msxCommandController,
                   MSXEventDistributor& msxEventDistributor,
                   Scheduler& scheduler, MSXCliComm& cliComm_,
                   Debugger& debugger_)
	: RecordedCommand(msxCommandController, msxEventDistributor,
	                  scheduler, "debug")
	, cliComm(cliComm_)
	, debugger(debugger_)
{
}

bool DebugCmd::needRecord(const vector<TclObject*>& tokens) const
{
	if (tokens.size() < 2) return false;
	string subCmd = tokens[1]->getString();
	return (subCmd == "write") || (subCmd == "write_block");
}

void DebugCmd::execute(const vector<TclObject*>& tokens,
                       TclObject& result, const EmuTime& /*time*/)
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
		bp.reset(new BreakPoint(cliComm, addr, command, condition));
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

void DebugCmd::removeBreakPoint(const vector<TclObject*>& tokens,
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

void DebugCmd::listBreakPoints(const vector<TclObject*>& /*tokens*/,
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

void DebugCmd::setWatchPoint(const vector<TclObject*>& tokens,
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

void DebugCmd::removeWatchPoint(const vector<TclObject*>& tokens,
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

void DebugCmd::listWatchPoints(const vector<TclObject*>& /*tokens*/,
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

string DebugCmd::help(const vector<string>& tokens) const
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
		"  The offset must be smaller then the value returned from the "
		"'size' subcommand\n"
		"  Note that openMSX comes with a bunch of TCL scripts that make "
		"some of the debug reads much more convient (e.g. reading from "
		"Z80 or VDP registers). See the 'commands.txt' document for more "
		"details about these.\n";
	static const string writeHelp =
		"debug write <name> <addr> <val>\n"
		"  Write a byte to the given debuggable at a certain offset.\n"
		"  The offset must be smaller then the value returned from the "
		"'size' subcommand\n";
	static const string readBlockHelp =
		"debug read_block <name> <addr> <size>\n"
		"  Read a whole block at once. This is equivalent with repeated "
		"invokations of the 'read' subcommand, but using this subcommand "
		"may be faster. The result is a TCL binary string (see TCL manual).\n"
		"  The block is specified as size/offset in the debuggable. The "
		"complete block must fit in the debuggable (see the 'size' "
		"subcommand).\n";
	static const string writeBlockHelp =
		"debug write_block <name> <addr> <values>\n"
		"  Write a whole block at once. This is equivalent with repeated "
		"invokations of the 'write' subcommand, but using this subcommand "
		"may be faster. The <values> arugment must be a TCL binary string "
		"(see TCL manual).\n"
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
		"  A condition must be specified as a TCL expression. For example\n"
		"     debug set_bp 0xf37d {[reg C] == 0x2F}\n"
		"  This break on address on address 0xf37d but only when Z80 "
		"register C has the value 0x2F.\n"
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
		"columns. The first column contains the brekpoint ID. The "
		"second one has the address. The third has the condition "
		"(default condition is empty). And the last column contains "
		"the command that will be executed (default is 'debug break').\n";
	static const string setWatchPointHelp =
		"debug set_watchpoint <type> <addr> [<cond>] [<cmd>]\n"
		"  Insert a new watchpoint of given type at the given address, "
		"there can be an optional condition and alternative command. See "
		"the 'set_bp' subcommand for details about these last two.\n"
		"  Type must be one of the following:\n"
		"    read_io    break when CPU reads from given IO port\n"
		"    write_io   break when CPU writes to given IO port\n"
		"    read_mem   break when CPU reads from given memory location\n"
		"    write_mem  break when CPU writes to given memory location\n"
		"  Example:\n"
		"    debug set_watchpoint write_io 0x99 {[reg A] == 0x81}\n";
	static const string removeWatchPointHelp =
		"debug remove_watchpoint <id>\n"
		"  Remove the watchpoint with given ID again. You can use the "
		"'list_watchpoints' subcommand to see all valid IDs.\n";
	static const string listWatchPointsHelp =
		"debug list_watchpoints\n"
		"  Lists all active breakpoints. The result is similar to the "
		"'list_bp' subcommand, but there is an extra column (2nd column) "
		"that contains the type of the watchpoint.\n";
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
		"is a TCL list. The first element in the list contains a textual "
		"representation of the instruction, the next elements contain the "
		"bytes that make up this instruction (thus the length of the "
		"resulting list can be used to derive the number of bytes in the "
		"instruction).\n"
		"  Note that openMSX comes with a 'disasm' TCL script that is much "
		"more convient to use then this subcommand.";
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
set<string> DebugCmd::getWatchPointIdsAsStringSet() const
{
	MSXCPUInterface& interface = debugger.motherBoard.getCPUInterface();
	const MSXCPUInterface::WatchPoints& watchPoints = interface.getWatchPoints();
	set<string> wpids;
	for (MSXCPUInterface::WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		wpids.insert("wp#" + StringOp::toString((*it)->getId()));
	}
	return wpids;
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
					set<string> wpids = getWatchPointIdsAsStringSet();
					completeString(tokens, wpids);
				} else if (tokens[1] == "set_watchpoint") {
					set<string> types;
					types.insert("write_io");
					types.insert("write_mem");
					types.insert("read_io");
					types.insert("read_mem");
					completeString(tokens, types);
				}
			}
			break;
		// other stuff doesn't really make sense to complete
		}
	}
}

} // namespace openmsx
