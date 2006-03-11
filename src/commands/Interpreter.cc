// $Id$

#include "Interpreter.hh"
#include "EventDistributor.hh"
#include "Command.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "Setting.hh"
#include "InterpreterOutput.hh"
#include "openmsx.hh"
#include "FileOperations.hh"
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
	const_cast<char*>("openMSX console"),// Type name
	NULL,			 // Always non-blocking
	dummyClose,		 // Close proc
	dummyInput,		 // Input proc
	Interpreter::outputProc, // Output proc
	NULL,			 // Seek proc
	NULL,			 // Set option proc
	NULL,			 // Get option proc
	dummyWatch,		 // Watch for events on console
	dummyGetHandle,		 // Get a handle from the device
	NULL,			 // Tcl_DriverClose2Proc
	NULL,			 // Tcl_DriverBlockModeProc
	NULL,			 // Tcl_DriverFlushProc
	NULL,			 // Tcl_DriverHandlerProc
	NULL,			 // Tcl_DriverWideSeekProc
};

void Interpreter::init(const char* programName)
{
	Tcl_FindExecutable(programName);
}

Interpreter::Interpreter(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
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
		"openMSX console", this, TCL_WRITABLE);
	if (channel != NULL) {
		Tcl_SetChannelOption(interp, channel, "-translation", "binary");
		Tcl_SetChannelOption(interp, channel, "-buffering", "line");
		Tcl_SetChannelOption(interp, channel, "-encoding", "binary");
	}
	Tcl_SetStdChannel(channel, TCL_STDOUT);

	setVariable("env(OPENMSX_USER_DATA)", FileOperations::getUserDataDir());
	setVariable("env(OPENMSX_SYSTEM_DATA)", FileOperations::getSystemDataDir());

	eventDistributor.registerEventListener(OPENMSX_POLL_EVENT, *this);

}

Interpreter::~Interpreter()
{
	eventDistributor.unregisterEventListener(OPENMSX_POLL_EVENT, *this);

	if (!Tcl_InterpDeleted(interp)) {
		Tcl_DeleteInterp(interp);
	}
	Tcl_Release(interp);

	Tcl_Finalize();
}

void Interpreter::setOutput(InterpreterOutput* output_)
{
	output = output_;
}

int Interpreter::outputProc(ClientData clientData, const char* buf,
                 int toWrite, int* /*errorCodePtr*/)
{
	try {
		InterpreterOutput* output = ((Interpreter*)clientData)->output;

		string text(buf, toWrite);
		if (!text.empty() && output) {
			output->output(text);
		}
	} catch (...) {
		assert(false); // we cannot let exceptions pass through TCL
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
	CommandTokenMap::iterator it = commandTokenMap.find(name);
	assert(it != commandTokenMap.end());
	Tcl_DeleteCommandFromToken(interp, it->second);
	commandTokenMap.erase(it);
}

int Interpreter::commandProc(ClientData clientData, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[])
{
	try {
		Command& command = *static_cast<Command*>(clientData);
		vector<TclObject*> tokens;
		tokens.reserve(objc);
		for (int i = 0; i < objc; ++i) {
			tokens.push_back(new TclObject(interp, objv[i]));
		}
		TclObject result(interp, Tcl_GetObjResult(interp));
		int res = TCL_OK;
		try {
			command.execute(tokens, result);
		} catch (MSXException& e) {
			result.setString(e.getMessage());
			res = TCL_ERROR;
		}
		for (vector<TclObject*>::const_iterator it = tokens.begin();
		     it != tokens.end(); ++it) {
			delete *it;
		}
		return res;
	} catch (...) {
		assert(false); // we cannot let exceptions pass through TCL
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
	result.insert(&argv[0], &argv[argc]);
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
	try {
		static string static_string;

		Setting* variable = static_cast<Setting*>(clientData);

		if (flags & TCL_TRACE_READS) {
			Tcl_SetVar(interp, part1,
			           variable->getValueString().c_str(), 0);
		}
		if (flags & TCL_TRACE_WRITES) {
			try {
				string newValue = Tcl_GetVar(interp, part1, 0);
				variable->setValueString(newValue);
				string newValue2 = variable->getValueString();
				if (newValue != newValue2) {
					Tcl_SetVar(interp, part1,
					           newValue2.c_str(), 0);
				}
			} catch (MSXException& e) {
				Tcl_SetVar(interp, part1,
				           variable->getValueString().c_str(), 0);
				static_string = e.getMessage();
				return const_cast<char*>(static_string.c_str());
			}
		}
		if (flags & TCL_TRACE_UNSETS) {
			variable->restoreDefault();
			Tcl_SetVar(interp, part1,
			           variable->getValueString().c_str(), 0);
			Tcl_TraceVar(interp, part1, TCL_TRACE_READS |
			                TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
			             traceProc,
			             static_cast<ClientData>(variable));
		}
	} catch (...) {
		assert(false); // we cannot let exceptions pass through TCL
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

void Interpreter::signalEvent(const Event& event)
{
	(void)event;
	assert(event.getType() == OPENMSX_POLL_EVENT);
	poll();
}

void Interpreter::poll()
{
	//Tcl_ServiceAll();
	Tcl_DoOneEvent(TCL_DONT_WAIT);
}

} // namespace openmsx
