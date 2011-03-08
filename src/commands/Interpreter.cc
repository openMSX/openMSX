#include "Interpreter.hh"
#include "EventDistributor.hh"
#include "Command.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXCommandController.hh"
#include "MSXMotherBoard.hh"
#include "Setting.hh"
#include "InterpreterOutput.hh"
#include "MSXCPUInterface.hh"
#include "openmsx.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <iostream>
//#include <tk.h>
#include "openmsx.hh"

using std::string;
using std::vector;

namespace openmsx {

// See comments in traceProc()
static std::map<long, Setting*> traceMap;
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
	nullptr,		 // Always non-blocking
	dummyClose,		 // Close proc
	dummyInput,		 // Input proc
	Interpreter::outputProc, // Output proc
	nullptr,		 // Seek proc
	nullptr,		 // Set option proc
	nullptr,		 // Get option proc
	dummyWatch,		 // Watch for events on console
	dummyGetHandle,		 // Get a handle from the device
	nullptr,		 // Tcl_DriverClose2Proc
	nullptr,		 // Tcl_DriverBlockModeProc
	nullptr,		 // Tcl_DriverFlushProc
	nullptr,		 // Tcl_DriverHandlerProc
	nullptr,		 // Tcl_DriverWideSeekProc
	nullptr,		 // Tcl_DriverThreadActionProc
	nullptr,		 // Tcl_DriverTruncateProc
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
	if (channel) {
		Tcl_SetChannelOption(interp, channel, "-translation", "binary");
		Tcl_SetChannelOption(interp, channel, "-buffering", "line");
		Tcl_SetChannelOption(interp, channel, "-encoding", "utf-8");
	}
	Tcl_SetStdChannel(channel, TCL_STDOUT);

	setVariable("env(OPENMSX_USER_DATA)", FileOperations::getUserDataDir());
	setVariable("env(OPENMSX_SYSTEM_DATA)", FileOperations::getSystemDataDir());

	eventDistributor.registerEventListener(OPENMSX_POLL_EVENT, *this);

}

Interpreter::~Interpreter()
{
	// see comment in MSXCPUInterface::cleanup()
	MSXCPUInterface::cleanup();

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
		auto* output = static_cast<Interpreter*>(clientData)->output;
		string_ref text(buf, toWrite);
		if (!text.empty() && output) {
			output->output(text);
		}
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
	}
	return toWrite;
}

void Interpreter::registerCommand(const string& name, Command& command)
{
	assert(commandTokenMap.find(name) == commandTokenMap.end());
	commandTokenMap[name] = Tcl_CreateObjCommand(
		interp, name.c_str(), commandProc,
		static_cast<ClientData>(&command), nullptr);
}

void Interpreter::unregisterCommand(string_ref name, Command& /*command*/)
{
	auto it = commandTokenMap.find(name);
	assert(it != commandTokenMap.end());
	Tcl_DeleteCommandFromToken(interp, it->second);
	commandTokenMap.erase(it);
}

int Interpreter::commandProc(ClientData clientData, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[])
{
	try {
		auto& command = *static_cast<Command*>(clientData);
		vector<TclObject> tokens;
		tokens.reserve(objc);
		for (auto i : xrange(objc)) {
			tokens.push_back(TclObject(interp, objv[i]));
		}
		int res = TCL_OK;
		TclObject result(interp);
		try {
			if (!command.isAllowedInEmptyMachine()) {
				if (auto controller =
					dynamic_cast<MSXCommandController*>(
						&command.getCommandController())) {
					if (!controller->getMSXMotherBoard().getMachineConfig()) {
						throw CommandException(
							"Can't execute command in empty machine");
					}
				}
			}
			command.execute(tokens, result);
		} catch (MSXException& e) {
			PRT_DEBUG(
				"Interpreter: Got an exception while executing a command: "
				<< e.getMessage());
			result.setString(e.getMessage());
			res = TCL_ERROR;
		}
		Tcl_SetObjResult(interp, result.getTclObject());
		return res;
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
		return TCL_ERROR;
	}
}

// Returns
//   - build-in Tcl commands
//   - openmsx commands
//   - user-defined procs
vector<string> Interpreter::getCommandNames()
{
	return splitList(execute("info commands"), interp);
}

bool Interpreter::isComplete(const string& command) const
{
	return Tcl_CommandComplete(command.c_str()) != 0;
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

static void setVar(Tcl_Interp* interp, const char* name, const char* value)
{
	if (!Tcl_SetVar(interp, name, value, TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) {
		// might contain error message of a trace proc
		std::cerr << Tcl_GetStringResult(interp) << std::endl;
	}
}
static const char* getVar(Tcl_Interp* interp, const char* name)
{
	return Tcl_GetVar(interp, name, TCL_GLOBAL_ONLY);
}

void Interpreter::setVariable(const string& name, const string& value)
{
	if (!Tcl_SetVar(interp, name.c_str(), value.c_str(),
		        TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
}

void Interpreter::unsetVariable(const string& name)
{
	Tcl_UnsetVar(interp, name.c_str(), TCL_GLOBAL_ONLY);
}

const char* Interpreter::getVariable(const string& name) const
{
	return getVar(interp, name.c_str());
}

static string getSafeValueString(Setting& setting)
{
	try {
		return setting.getValueString();
	} catch (MSXException&) {
		return "0"; // 'safe' value, see comment in registerSetting()
	}
}
void Interpreter::registerSetting(Setting& variable, const string& name)
{
	if (const char* tclVarValue = getVariable(name)) {
		// Tcl var already existed, use this value
		try {
			variable.setValueStringDirect(tclVarValue);
		} catch (MSXException&) {
			// Ignore: can happen in case of proxy settings when
			// the current machine doesn't have this setting.
			// E.g.
			//   (start with cbios machine)
			//   set renshaturbo 0
			//   create_machine
			//   machine2::load_machine Panasonic_FS-A1GT
		}
	} else {
		// define Tcl var
		setVariable(name, getSafeValueString(variable));
	}

	// The call setVariable() above can already trigger traces on this
	// variable (in Tcl it's possible to already set traces on a variable
	// before that variable is defined). We didn't yet set a trace on it
	// ourselves. So for example on proxy-settings we don't yet delegate
	// read/writes to the actual setting. This means that inside the trace
	// callback we see the value set above instead of the 'actual' value.
	//
	// This scenario can be triggered in the load_icons script by
	// executing the following commands (interactively):
	//   delete_machine machine1
	//   create_machine
	//   machine2::load_machine msx2
	//
	// Before changing the 'safe-value' (see getSafeValueString()) to '0',
	// this gave errors because the load_icons script didn't expect to see
	// 'proxy' (the old 'safe-value') as value.
	//
	// The current solution (choosing '0' as safe value) is not ideal, but
	// good enough for now.
	//
	// A possible better solution is to move Tcl_TraceVar() before
	// setVariable(), I did an initial attempt but there were some
	// problems. TODO investigate this further.

	long traceID = traceCount++;
	traceMap[traceID] = &variable;
	Tcl_TraceVar(interp, name.c_str(),
	             TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	             traceProc, reinterpret_cast<ClientData>(traceID));
}

void Interpreter::unregisterSetting(Setting& variable, const string& name)
{
	auto it = traceMap.begin();
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
	auto it = traceMap.find(traceID);
	return (it != traceMap.end()) ? it->second : nullptr;
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

		auto traceID = reinterpret_cast<long>(clientData);
		auto* variable = getTraceSetting(traceID);
		if (!variable) return nullptr;

		static string static_string;
		if (flags & TCL_TRACE_READS) {
			try {
				setVar(interp, part1, variable->getValueString().c_str());
			} catch (MSXException& e) {
				static_string = e.getMessage();
				return const_cast<char*>(static_string.c_str());
			}
		}
		if (flags & TCL_TRACE_WRITES) {
			try {
				const char* v = getVar(interp, part1);
				string newValue = v ? v : "";
				variable->setValueStringDirect(newValue);
				string newValue2 = variable->getValueString();
				if (newValue != newValue2) {
					setVar(interp, part1, newValue2.c_str());
				}
			} catch (MSXException& e) {
				setVar(interp, part1, getSafeValueString(*variable).c_str());
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
			} catch (MSXException&) {
				// for some reason default value is not valid ATM,
				// keep current value (happened for videosource
				// setting before turning on (set power on) the
				// MSX machine)
			}
			setVar(interp, part1, getSafeValueString(*variable).c_str());
			Tcl_TraceVar(interp, part1, TCL_TRACE_READS |
			                TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
			             traceProc,
			             reinterpret_cast<ClientData>(traceID));
		}
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
	}
	return nullptr;
}

void Interpreter::createNamespace(const std::string& name)
{
	execute("namespace eval ::" + name + " {}");
}

void Interpreter::deleteNamespace(const std::string& name)
{
	execute("namespace delete ::" + name);
}

vector<string> Interpreter::splitList(const std::string& list)
{
	return splitList(list, interp);
}

vector<string> Interpreter::splitList(const string& list, Tcl_Interp* interp)
{
	int argc;
	const char** argv;
	if (Tcl_SplitList(interp, list.c_str(), &argc, &argv) == TCL_ERROR) {
		throw CommandException(
			interp ? Tcl_GetStringResult(interp)
			       : "splitList error");
	}
	vector<string> result(argv, argv + argc);
	Tcl_Free(reinterpret_cast<char*>(argv));
	return result;
}

int Interpreter::signalEvent(const std::shared_ptr<const Event>& event)
{
	(void)event;
	assert(event->getType() == OPENMSX_POLL_EVENT);
	poll();
	return 0;
}

void Interpreter::poll()
{
	//Tcl_ServiceAll();
	Tcl_DoOneEvent(TCL_DONT_WAIT);
}

TclParser Interpreter::parse(string_ref command)
{
	return TclParser(interp, command);
}

} // namespace openmsx
