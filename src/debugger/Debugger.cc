// $Id$

#include <cassert>
#include <sstream>
#include "CommandController.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "CommandResult.hh"

using std::ostringstream;

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

void Debugger::DebugCmd::execute(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "read") {
		read(tokens, result);
		return;
	} else if (tokens[1] == "read_block") {
		readBlock(tokens, result);
		return;
	} else if (tokens[1] == "write") {
		write(tokens, result);
		return;
	} else if (tokens[1] == "write_block") {
		writeBlock(tokens, result);
		return;
	} else if (tokens[1] == "size") {
		size(tokens, result);
		return;
	} else if (tokens[1] == "desc") {
		desc(tokens, result);
		return;
	} else if (tokens[1] == "list") {
		list(result);
		return;
	} else if (tokens[1] == "step") {
		parent.cpu->doStep();
		return;
	} else if (tokens[1] == "cont") {
		parent.cpu->doContinue();
		return;
	} else if (tokens[1] == "break") {
		parent.cpu->doBreak();
		return;
	} else if (tokens[1] == "set_bp") {
		parent.cpu->setBreakPoint(tokens);
		return;
	} else if (tokens[1] == "remove_bp") {
		parent.cpu->removeBreakPoint(tokens);
		return;
	} else if (tokens[1] == "list_bp") {
		result.setString(parent.cpu->listBreakPoints());
		return;
	}
	throw SyntaxError();
}

static unsigned getValue(const string& str, unsigned max, const char* err)
{
	char* endPtr;
	unsigned result = strtoul(str.c_str(), &endPtr, 0);
	if ((*endPtr != '\0') || (result >= max)) {
		throw CommandException(err);
	}
	return result;
}

void Debugger::DebugCmd::list(CommandResult& result)
{
	for (map<string, Debuggable*>::const_iterator it =
	       parent.debuggables.begin();
	     it != parent.debuggables.end(); ++it) {
		result.addListElement(it->first);
	}
}

void Debugger::DebugCmd::desc(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	result.setString(device->getDescription());
}

void Debugger::DebugCmd::size(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	result.setInt(device->getSize());
}

void Debugger::DebugCmd::read(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 4) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	unsigned addr = getValue(tokens[3], device->getSize(), "Invalid address");
	result.setInt(device->read(addr));
}

void Debugger::DebugCmd::readBlock(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	unsigned size = device->getSize();
	unsigned addr = getValue(tokens[3], size, "Invalid address");
	unsigned num = getValue(tokens[4], size - addr + 1, "Invalid size");

	byte* buf = new byte[num];
	for (unsigned i = 0; i < num; ++i) {
		buf[i] = device->read(addr + i);
	}
	result.setBinary(buf, num);
	delete[] buf;
}

void Debugger::DebugCmd::write(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	unsigned addr = getValue(tokens[3], device->getSize(), "Invalid address");
	unsigned value = getValue(tokens[4], 256, "Invalid value");
	
	device->write(addr, value);
}

void Debugger::DebugCmd::writeBlock(const vector<string>& tokens, CommandResult& result)
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	Debuggable* device = parent.getDebuggable(tokens[2]);
	unsigned size = device->getSize();
	unsigned addr = getValue(tokens[3], size, "Invalid address");
	const string& values = tokens[4];
	unsigned num = values.size();
	if ((num + addr) > size) {
		throw CommandException("Invalid size");
	}
	
	for (unsigned i = 0; i < num; ++i) {
		device->write(addr + i, static_cast<byte>(values[i]));
	}
}

string Debugger::DebugCmd::help(const vector<string>& tokens) const
{
	static const string helpText =
		"debug list                       returns a list of all debuggables\n"
		"debug desc <name>                returns a description of this debuggable\n"
		"debug size <name>                returns the size of this debuggable\n"
		"debug read <name> <addr>         read a byte from a debuggable\n"
		"debug write <name> <addr> <val>  write a byte to a debuggable\n" 
		"debug set_bp <addr>              insert a new breakpoint\n"
		"debug remove_bp <addr>           remove a certain breapoint\n"
		"debug list_bp                    list the active breakpoints\n"
		"debug cont                       continue execution aftre break\n"
		"debug step                       execute one instruction\n"
		"debug break                      break CPU at current position\n";
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
