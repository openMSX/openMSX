// $Id$

#include "openmsx.hh"
#include "Command.hh"
#include "CommandConsole.hh"
#include "CommandArgument.hh"
#include "CommandException.hh"
#include "Setting.hh"
#include "Scheduler.hh"
#include "Interpreter.hh"
//#include <tk.h>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

static int dummyClose(ClientData /*instanceData*/, Tcl_Interp* /*interp*/)
{
	return 0;
}
static int dummyInput(ClientData /*instanceData*/, char* /*buf*/,
                      int /*bufSize*/, int* /*errorCodePtr*/)
{
	return 0;
}
static void dummyWatch(ClientData /*instanceData*/, int /*mask*/)
{
}
static int dummyGetHandle(ClientData /*instanceData*/, int /*direction*/,
                          ClientData* /*handlePtr*/)
{
	return TCL_ERROR;
}
Tcl_ChannelType Interpreter::channelType = {
	"openMSX console",	// Type name
	NULL,			// Always non-blocking
	dummyClose,		// Close proc
	dummyInput,		// Input proc
	Interpreter::outputProc,// Output proc
	NULL,			// Seek proc
	NULL,			// Set option proc
	NULL,			// Get option proc
	dummyWatch,		// Watch for events on console
	dummyGetHandle,		// Get a handle from the device
	NULL,			// Tcl_DriverClose2Proc
	NULL,			// Tcl_DriverBlockModeProc
	NULL,			// Tcl_DriverFlushProc
	NULL,			// Tcl_DriverHandlerProc
	NULL,			// Tcl_DriverWideSeekProc
};

void Interpreter::init(const char* programName)
{
	Tcl_FindExecutable(programName);
}

Interpreter::Interpreter()
{
	interp = Tcl_CreateInterp();
	Tcl_Preserve(interp);
	
	// TODO need to investigate this: doesn't work on windows
	/*
	if (Tcl_Init(interp) != TCL_OK) {
		std::cout << "Tcl_Init: " << interp->result << std::endl;
	}
	if (Tk_Init(interp) != TCL_OK) {
		std::cout << "Tk_Init error: " << interp->result << std::endl;
	}
	if (Tcl_Eval(interp, "wm withdraw .") != TCL_OK) {
		std::cout << "wm withdraw error: " << interp->result << std::endl;
	}
	*/

	Tcl_Channel channel = Tcl_CreateChannel(&channelType,
		"openMSX console", NULL, TCL_WRITABLE);
	if (channel != NULL) {
		Tcl_SetChannelOption(interp, channel, "-translation", "binary");
		Tcl_SetChannelOption(interp, channel, "-buffering", "line");
		Tcl_SetChannelOption(interp, channel, "-encoding", "binary");
	}
	Tcl_SetStdChannel(channel, TCL_STDOUT);
}

Interpreter::~Interpreter()
{
	if (!Tcl_InterpDeleted(interp)) {
		Tcl_DeleteInterp(interp);
	}
	Tcl_Release(interp);
}

Interpreter& Interpreter::instance()
{
	static Interpreter oneInstance;
	return oneInstance;
}

int Interpreter::outputProc(ClientData /*clientData*/, const char* buf,
                 int toWrite, int* /*errorCodePtr*/)
{
	string output(buf, toWrite);
	if (!output.empty()) {
		CommandConsole::instance().print(output);
	}
	return toWrite;
}

void Interpreter::registerCommand(const string& name, Command& command)
{
	assert(commandTokenMap.find(name) == commandTokenMap.end());
	commandTokenMap[name] = Tcl_CreateObjCommand(
		interp, name.c_str(), commandProc,
		static_cast<ClientData>(&command), NULL);
}

void Interpreter::unregisterCommand(const string& name, Command& /*command*/)
{
	assert(commandTokenMap.find(name) != commandTokenMap.end());
	Tcl_DeleteCommandFromToken(interp, commandTokenMap[name]);
}

int Interpreter::commandProc(ClientData clientData, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[])
{
	Command& command = *static_cast<Command*>(clientData);
	vector<CommandArgument> tokens;
	tokens.reserve(objc);
	for (int i = 0; i < objc; ++i) {
		tokens.push_back(CommandArgument(interp, objv[i]));
	}
	CommandResult result(interp);
	try {
		command.execute(tokens, result);
		return TCL_OK;
	} catch (MSXException& e) {
		result.setString(e.getMessage());
		return TCL_ERROR;
	}
}

// Returns
//   - build-in TCL commands
//   - openmsx commands
//   - user-defined procs
void Interpreter::getCommandNames(set<string>& result)
{
	string list = execute("info commands");
	
	int argc;
	const char** argv;
	if (Tcl_SplitList(interp, list.c_str(), &argc, &argv) != TCL_OK) {
		return;
	}
	for (int i = 0; i < argc; ++i) {
		result.insert(argv[i]);
	}
	Tcl_Free((char*)argv);
}

bool Interpreter::isComplete(const string& command) const
{
	return Tcl_CommandComplete(command.c_str());
}

string Interpreter::execute(const string& command)
{
	int success = Tcl_Eval(interp, command.c_str());
	string result =  Tcl_GetStringResult(interp);
	if (success != TCL_OK) {
		throw CommandException(result);
	}
	return result;
}

string Interpreter::executeFile(const string& filename)
{
	int success = Tcl_EvalFile(interp, filename.c_str());
	string result =  Tcl_GetStringResult(interp);
	if (success != TCL_OK) {
		throw CommandException(result);
	}
	return result;
}

void Interpreter::setVariable(const string& name, const string& value)
{
	Tcl_SetVar(interp, name.c_str(), value.c_str(), 0);
}

void Interpreter::unsetVariable(const string& name)
{
	Tcl_UnsetVar(interp, name.c_str(), 0);
}

const char* Interpreter::getVariable(const string& name) const
{
	return Tcl_GetVar(interp, name.c_str(), 0);
}

void Interpreter::registerSetting(Setting& variable)
{
	const string& name = variable.getName();
	const char* tclVarValue = getVariable(name);
	if (tclVarValue) {
		// TCL var already existed, use this value
		variable.setValueString(tclVarValue);
	} else {
		// define TCL var
		setVariable(name, variable.getValueString());
	}
	Tcl_TraceVar(interp, name.c_str(),
	             TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	             traceProc, static_cast<ClientData>(&variable));
}

void Interpreter::unregisterSetting(Setting& variable)
{
	const string& name = variable.getName();
	Tcl_UntraceVar(interp, name.c_str(),
	               TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	               traceProc, static_cast<ClientData>(&variable));
	unsetVariable(name);
}

char* Interpreter::traceProc(ClientData clientData, Tcl_Interp* interp,
                           const char* part1, const char* /*part2*/, int flags)
{
	static string static_string;
	
	Setting* variable = static_cast<Setting*>(clientData);
	
	if (flags & TCL_TRACE_READS) {
		Tcl_SetVar(interp, part1, variable->getValueString().c_str(), 0);
	}
	if (flags & TCL_TRACE_WRITES) {
		try {
			string newValue = Tcl_GetVar(interp, part1, 0);
			variable->setValueString(newValue);
			string newValue2 = variable->getValueString();
			if (newValue != newValue2) {
				Tcl_SetVar(interp, part1, newValue2.c_str(), 0);
			}
		} catch (MSXException& e) {
			Tcl_SetVar(interp, part1, variable->getValueString().c_str(), 0);
			static_string = e.getMessage();
			return const_cast<char*>(static_string.c_str());
		}
	}
	if (flags & TCL_TRACE_UNSETS) {
		variable->restoreDefault();
		Tcl_SetVar(interp, part1, variable->getValueString().c_str(), 0);
		Tcl_TraceVar(interp, part1,
		             TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
		             traceProc, static_cast<ClientData>(variable));
	}
	return NULL;
}

void Interpreter::splitList(const string& list, vector<string>& result)
{
	int argc;
	const char** argv;
	if (Tcl_SplitList(interp, list.c_str(), &argc, &argv) == TCL_ERROR) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	result.assign(argv, argv + argc);
	Tcl_Free((char*)argv);
}


void Interpreter::poll()
{
	//Tcl_ServiceAll();
	Tcl_DoOneEvent(TCL_DONT_WAIT);
}

} // namespace openmsx
