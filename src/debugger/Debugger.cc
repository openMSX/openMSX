// $Id$

#include "CommandController.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "BreakPoint.hh"
#include "TclObject.hh"
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

Debugger::Debugger()
	: debugCmd(*this),
	  commandController(CommandController::instance()),
	  cpu(0)
{
	commandController.registerCommand(&debugCmd, "debug");
}

Debugger::~Debugger()
{
	assert(!cpu);
	assert(debuggables.empty());
	commandController.unregisterCommand(&debugCmd, "debug");
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
	if (&debuggable); // avoid warning
	map<string, Debuggable*>::iterator it = debuggables.find(name);
	assert(it != debuggables.end() && (it->second == &debuggable));
	debuggables.erase(it);
}

Debuggable* Debugger::getDebuggable(const string& name)
{
	map<string, Debuggable*>::iterator it = debuggables.find(name);
	if (it == debuggables.end()) {
		throw CommandException("No such debuggable.");
	}
	return it->second;
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

Debugger::DebugCmd::DebugCmd(Debugger& parent_)
	: parent(parent_)
{
}

void Debugger::DebugCmd::execute(const vector<TclObject*>& tokens,
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
		parent.cpu->doStep();
	} else if (subCmd == "cont") {
		parent.cpu->doContinue();
	} else if (subCmd == "disasm") {
		parent.cpu->disasmCommand(tokens, result);
	} else if (subCmd == "break") {
		parent.cpu->doBreak();
	} else if (subCmd == "breaked") {
		result.setInt(parent.cpu->isBreaked());
	} else if (subCmd == "set_bp") {
		setBreakPoint(tokens, result);
	} else if (subCmd == "remove_bp") {
		removeBreakPoint(tokens, result);
	} else if (subCmd == "list_bp") {
		listBreakPoints(tokens, result);
	} else {
		throw SyntaxError();
	}
}

void Debugger::DebugCmd::list(TclObject& result)
{
	for (map<string, Debuggable*>::const_iterator it =
	       parent.debuggables.begin();
	     it != parent.debuggables.end(); ++it) {
		result.addListElement(it->first);
	}
}

void Debugger::DebugCmd::desc(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
	result.setString(device->getDescription());
}

void Debugger::DebugCmd::size(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
	result.setInt(device->getSize());
}

void Debugger::DebugCmd::read(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
	unsigned addr = tokens[3]->getInt();
	if (addr >= device->getSize()) {
		throw CommandException("Invalid address");
	}
	result.setInt(device->read(addr));
}

void Debugger::DebugCmd::readBlock(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
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

void Debugger::DebugCmd::write(const vector<TclObject*>& tokens,
                               TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
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

void Debugger::DebugCmd::writeBlock(const vector<TclObject*>& tokens,
                                    TclObject& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]->getString());
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

void Debugger::DebugCmd::setBreakPoint(const vector<TclObject*>& tokens,
                                       TclObject& result)
{
	word addr = getAddress(tokens);
	auto_ptr<BreakPoint> bp;
	switch (tokens.size()) {
	case 3: // no condition
		bp.reset(new BreakPoint(addr));
		break;
	case 4: { // contional bp
		auto_ptr<TclObject> cond(new TclObject(*tokens[3]));
		cond->checkExpression();
		bp.reset(new BreakPoint(addr, cond));
		break;
	}
	default:
		throw CommandException("Too many arguments.");
	}
	result.setString("bp#" + StringOp::toString(bp->getId()));
	parent.cpu->insertBreakPoint(bp);
}

void Debugger::DebugCmd::removeBreakPoint(const std::vector<TclObject*>& tokens,
                                          TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	const CPU::BreakPoints& breakPoints = parent.cpu->getBreakPoints();
	
	string tmp = tokens[2]->getString();
	if (StringOp::startsWith(tmp, "bp#")) {
		// remove by id
		unsigned id = StringOp::stringToInt(tmp.substr(3));
		for (CPU::BreakPoints::const_iterator it = breakPoints.begin();
		     it != breakPoints.end(); ++it) {
			const BreakPoint& bp = *it->second;
			if (bp.getId() == id) {
				parent.cpu->removeBreakPoint(bp);
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
				parent.cpu->removeBreakPoint(bp);
				return;
			}
		}
		throw CommandException(
			"No (unconditional) breakpoint at address: " + tmp);
	}
}

void Debugger::DebugCmd::listBreakPoints(const std::vector<TclObject*>& /*tokens*/,
                                         TclObject& result)
{
	const CPU::BreakPoints& breakPoints = parent.cpu->getBreakPoints();
	ostringstream os;
	os.fill('0');
	for (CPU::BreakPoints::const_iterator it = breakPoints.begin();
	     it != breakPoints.end(); ++it) {
		const BreakPoint& bp = *it->second;
		os << "bp#" << bp.getId() << " "
		      "0x" << std::hex << std::setw(4) << bp.getAddress() <<
		      ' ' << bp.getCondition() << '\n';
	}
	result.setString(os.str());
}

string Debugger::DebugCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText =
		"debug list                                returns a list of all debuggables\n"
		"debug desc <name>                         returns a description of this debuggable\n"
		"debug size <name>                         returns the size of this debuggable\n"
		"debug read <name> <addr>                  read a byte from a debuggable\n"
		"debug write <name> <addr> <val>           write a byte to a debuggable\n"
		"debug read_block <name> <addr> <size>     read a whole block at once\n"
		"debug write_block <name> <addr> <values>  write a whole block at once\n"
		"debug set_bp <addr> [<condition>]         insert a new breakpoint\n"
		"debug remove_bp <id>                      remove a certain breapoint\n"
		"debug list_bp                             list the active breakpoints\n"
		"debug cont                                continue execution aftre break\n"
		"debug step                                execute one instruction\n"
		"debug break                               break CPU at current position\n"
		"debug breaked                             query CPU breaked status\n"
		"debug disasm <addr>                       disassemble instruction\n";
	return helpText;
}

void Debugger::DebugCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
		case 2: {
			set<string> cmds;
			cmds.insert("list");
			cmds.insert("desc");
			cmds.insert("size");
			cmds.insert("read");
			cmds.insert("read_block");
			cmds.insert("write");
			cmds.insert("write_block");
			cmds.insert("step");
			cmds.insert("cont");
			cmds.insert("disasm");
			cmds.insert("break");
			cmds.insert("breaked");
			cmds.insert("set_bp");
			cmds.insert("remove_bp");
			cmds.insert("list_bp");
			CommandController::completeString(tokens, cmds);
			break;
		}
		case 3: {
			set<string> debuggables;
			parent.getDebuggables(debuggables);
			CommandController::completeString(tokens, debuggables);
			break;
		}
	}
}

} // namespace openmsx
