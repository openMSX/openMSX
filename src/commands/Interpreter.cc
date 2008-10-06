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
#include "StringOp.hh"
//#include <tk.h>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

// See comments in traceProc()
typedef std::map<long, Setting*> TraceMap;
static TraceMap traceMap;
static long traceCount = 0;


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
		InterpreterOutput* output =
			static_cast<Interpreter*>(clientData)->output;
		string text(buf, toWrite);
		if (!text.empty() && output) {
			output->output(text);
		}
	} catch (...) {
		assert(false); // we cannot let exceptions pass through Tcl
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
		int res = TCL_OK;
		TclObject result(interp);
		try {
			command.execute(tokens, result);
		} catch (MSXException& e) {
			result.setString(e.getMessage());
			res = TCL_ERROR;
		}
		Tcl_SetObjResult(interp, result.getTclObject());
		for (vector<TclObject*>::const_iterator it = tokens.begin();
		     it != tokens.end(); ++it) {
			delete *it;
		}
		return res;
	} catch (...) {
		assert(false); // we cannot let exceptions pass through Tcl
		return TCL_ERROR;
	}
}

// Returns
//   - build-in Tcl commands
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
	Tcl_Free(reinterpret_cast<char*>(argv));
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

void Interpreter::registerSetting(Setting& variable, const string& name)
{
	const char* tclVarValue = getVariable(name);
	if (tclVarValue) {
		// Tcl var already existed, use this value
		variable.setValueStringDirect(tclVarValue);
	} else {
		// define Tcl var
		setVariable(name, variable.getValueString());
	}
	long traceID = traceCount++;
	traceMap[traceID] = &variable;
	Tcl_TraceVar(interp, name.c_str(),
	             TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	             traceProc, reinterpret_cast<ClientData>(traceID));
}

void Interpreter::unregisterSetting(Setting& variable, const string& name)
{
	TraceMap::iterator it = traceMap.begin();
	while (true) {
		assert(it != traceMap.end());
		if (it->second == &variable) break;
		++it;
	}

	long traceID = it->first;
	Tcl_UntraceVar(interp, name.c_str(),
	               TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	               traceProc, reinterpret_cast<ClientData>(traceID));
	traceMap.erase(it);
	unsetVariable(name);
}

static Setting* getTraceSetting(unsigned traceID)
{
	TraceMap::const_iterator it = traceMap.find(traceID);
	return (it != traceMap.end()) ? it->second : NULL;
}

char* Interpreter::traceProc(ClientData clientData, Tcl_Interp* interp,
                           const char* part1, const char* /*part2*/, int flags)
{
	try {
		// Lookup Setting object that belongs to this Tcl variable.
		//
		// In a previous implementation we passed this object directly
		// as the clientData. However this went wrong in the following
		// scenario:
		//
		//    proc foo {} { carta eject ; carta spmanbow.rom }
		//    bind Q foo
		//    [press Q twice]
		//
		// The problem is that when a SCC cartridge is removed, we
		// delete several settings (e.g. SCC_ch1_mute). While deleting
		// a setting we unset the corresponsing Tcl variable (see
		// unregisterSetting() above), this in turn triggers a
		// TCL_TRACE_UNSET callback (this function). To prevent this
		// callback from triggering we first remove the trace before
		// unsetting the variable. However it seems when a setting is
		// deleted from within an active Tcl proc (like in the example
		// above), the callback is anyway triggered, but only at the
		// end of the proc (so in the foo proc above, the settings
		// are deleted after the first statement, but the callbacks
		// only happen after the second statement). By that time the
		// Setting object is already deleted and the callback function
		// works on a deleted object.
		//
		// To prevent this we don't anymore pass a pointer to the
		// Setting object as clientData, but we lookup the Setting in
		// a map. If the Setting was deleted, we won't find it anymore
		// in the map and return.

		long traceID = reinterpret_cast<long>(clientData);
		Setting* variable = getTraceSetting(traceID);
		if (!variable) return NULL;

		static string static_string;
		if (flags & TCL_TRACE_READS) {
			Tcl_SetVar(interp, part1,
			           variable->getValueString().c_str(), 0);
		}
		if (flags & TCL_TRACE_WRITES) {
			try {
				string newValue = Tcl_GetVar(interp, part1, 0);
				variable->setValueStringDirect(newValue);
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
			try {
				// note we cannot use restoreDefault(), because
				// that goes via Tcl and the Tcl variable
				// doesn't exist at this point
				variable->setValueStringDirect(
					variable->getRestoreValueString());
			} catch (CommandException& e) {
				// for some reason default value is not valid ATM,
				// keep current value (happened for videosource
				// setting before turning on (set power on) the
				// MSX machine)
			}
			Tcl_SetVar(interp, part1,
			           variable->getValueString().c_str(), 0);
			Tcl_TraceVar(interp, part1, TCL_TRACE_READS |
			                TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
			             traceProc,
			             reinterpret_cast<ClientData>(traceID));
		}
	} catch (...) {
		assert(false); // we cannot let exceptions pass through Tcl
	}
	return NULL;
}

void Interpreter::createNamespace(const std::string& name)
{
	execute("namespace eval " + name + " {}");
}

void Interpreter::deleteNamespace(const std::string& name)
{
	execute("namespace delete " + name);
}

void Interpreter::splitList(const std::string& list,
	                    std::vector<std::string>& result)
{
	splitList(list, result, interp);
}

void Interpreter::splitList(const string& list, vector<string>& result,
                            Tcl_Interp* interp)
{
	int argc;
	const char** argv;
	if (Tcl_SplitList(interp, list.c_str(), &argc, &argv) == TCL_ERROR) {
		string message = interp ? Tcl_GetStringResult(interp)
		                        : "splitList error";
		throw CommandException(message);
	}
	result.assign(argv, argv + argc);
	Tcl_Free(reinterpret_cast<char*>(argv));
}

bool Interpreter::signalEvent(shared_ptr<const Event> event)
{
	(void)event;
	assert(event->getType() == OPENMSX_POLL_EVENT);
	poll();
	return true;
}

void Interpreter::poll()
{
	//Tcl_ServiceAll();
	Tcl_DoOneEvent(TCL_DONT_WAIT);
}

} // namespace openmsx
