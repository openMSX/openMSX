// $Id$

#include <cassert>
#include <sstream>
#include "CommandController.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "CommandArgument.hh"

using std::map;
using std::ostringstream;
using std::set;
using std::string;
using std::vector;

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

Debugger& Debugger::instance()
{
	static Debugger oneInstance;
	return oneInstance;
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

Debugger::DebugCmd::DebugCmd(Debugger& parent_)
	: parent(parent_)
{
}

void Debugger::DebugCmd::execute(const vector<CommandArgument>& tokens,
                                 CommandArgument& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	string subCmd = tokens[1].getString();
	if (subCmd == "read") {
		read(tokens, result);
		return;
	} else if (subCmd == "read_block") {
		readBlock(tokens, result);
		return;
	} else if (subCmd == "write") {
		write(tokens, result);
		return;
	} else if (subCmd == "write_block") {
		writeBlock(tokens, result);
		return;
	} else if (subCmd == "size") {
		size(tokens, result);
		return;
	} else if (subCmd == "desc") {
		desc(tokens, result);
		return;
	} else if (subCmd == "list") {
		list(result);
		return;
	} else if (subCmd == "step") {
		parent.cpu->doStep();
		return;
	} else if (subCmd == "cont") {
		parent.cpu->doContinue();
		return;
	} else if (subCmd == "break") {
		parent.cpu->doBreak();
		return;
	} else if (subCmd == "set_bp") {
		unsigned addr = tokens[2].getInt();
		if (addr >= 0x10000) {
			throw CommandException("Invalid address");
		}
		parent.cpu->setBreakPoint(addr);
		return;
	} else if (subCmd == "remove_bp") {
		unsigned addr = tokens[2].getInt();
		if (addr >= 0x10000) {
			throw CommandException("Invalid address");
		}
		parent.cpu->removeBreakPoint(addr);
		return;
	} else if (subCmd == "list_bp") {
		result.setString(parent.cpu->listBreakPoints());
		return;
	}
	throw SyntaxError();
}

void Debugger::DebugCmd::list(CommandArgument& result)
{
	for (map<string, Debuggable*>::const_iterator it =
	       parent.debuggables.begin();
	     it != parent.debuggables.end(); ++it) {
		result.addListElement(it->first);
	}
}

void Debugger::DebugCmd::desc(const vector<CommandArgument>& tokens, CommandArgument& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	result.setString(device->getDescription());
}

void Debugger::DebugCmd::size(const vector<CommandArgument>& tokens, CommandArgument& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	result.setInt(device->getSize());
}

void Debugger::DebugCmd::read(const vector<CommandArgument>& tokens, CommandArgument& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	unsigned addr = tokens[3].getInt();
	if (addr >= device->getSize()) {
		throw CommandException("Invalid address");
	}
	result.setInt(device->read(addr));
}

void Debugger::DebugCmd::readBlock(const vector<CommandArgument>& tokens, CommandArgument& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	unsigned size = device->getSize();
	unsigned addr = tokens[3].getInt();
	if (addr >= size) {
		throw CommandException("Invalid address");
	}
	unsigned num = tokens[4].getInt();
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

void Debugger::DebugCmd::write(const vector<CommandArgument>& tokens,
                               CommandArgument& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	unsigned addr = tokens[3].getInt();
	if (addr >= device->getSize()) {
		throw CommandException("Invalid address");
	}
	unsigned value = tokens[4].getInt();
	if (value >= 256) {
		throw CommandException("Invalid value");
	}
	
	device->write(addr, value);
}

void Debugger::DebugCmd::writeBlock(const vector<CommandArgument>& tokens,
                                    CommandArgument& /*result*/)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2].getString());
	unsigned size = device->getSize();
	unsigned addr = tokens[3].getInt();
	if (addr >= size) {
		throw CommandException("Invalid address");
	}
	unsigned num;
	const byte* buf = tokens[4].getBinary(num);
	if ((num + addr) > size) {
		throw CommandException("Invalid size");
	}
	
	for (unsigned i = 0; i < num; ++i) {
		device->write(addr + i, static_cast<byte>(buf[i]));
	}
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
		"debug set_bp <addr>                       insert a new breakpoint\n"
		"debug remove_bp <addr>                    remove a certain breapoint\n"
		"debug list_bp                             list the active breakpoints\n"
		"debug cont                                continue execution aftre break\n"
		"debug step                                execute one instruction\n"
		"debug break                               break CPU at current position\n";
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
			cmds.insert("break");
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
