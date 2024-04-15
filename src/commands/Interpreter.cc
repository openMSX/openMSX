#include "Interpreter.hh"

#include "Command.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXCommandController.hh"
#include "MSXMotherBoard.hh"
#include "Setting.hh"
#include "InterpreterOutput.hh"
#include "MSXCPUInterface.hh"
#include "FileOperations.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"

#include <bit>
#include <cstdint>
#include <iostream>
#include <span>
#include <utility>
#include <vector>
//#include <tk.h>

namespace openmsx {

// See comments in traceProc()
namespace {
	struct Trace {
		uintptr_t id;
		BaseSetting* setting;
	};
}
static std::vector<Trace> traces; // sorted on id
static uintptr_t traceCount = 0;


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

void Interpreter::init(const char* programName) const
{
	Tcl_FindExecutable(programName);
}

Interpreter::Interpreter()
	: interp(Tcl_CreateInterp())
{
	Tcl_Preserve(interp);

	// TODO need to investigate this: doesn't work on windows
	/*
	if (Tcl_Init(interp) != TCL_OK) {
		std::cout << "Tcl_Init: " << interp->result << '\n';
	}
	if (Tk_Init(interp) != TCL_OK) {
		std::cout << "Tk_Init error: " << interp->result << '\n';
	}
	if (Tcl_Eval(interp, "wm withdraw .") != TCL_OK) {
		std::cout << "wm withdraw error: " << interp->result << '\n';
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

	setVariable(TclObject("env(OPENMSX_USER_DATA)"),
	            TclObject(FileOperations::getUserDataDir()));
	setVariable(TclObject("env(OPENMSX_SYSTEM_DATA)"),
	            TclObject(FileOperations::getSystemDataDir()));
}

Interpreter::~Interpreter()
{
	// see comment in MSXCPUInterface::cleanup()
	MSXCPUInterface::cleanup();

	if (!Tcl_InterpDeleted(interp)) {
		Tcl_DeleteInterp(interp);
	}
	Tcl_Release(interp);

	// Tcl_Finalize() should only be called once for the whole application
	// tcl8.6 checks for this (tcl8.5 did not).
	// Normally we only create/destroy exactly one Interpreter object for
	// openMSX, and then simply calling Tcl_Finalize() here is fine. Though
	// when running unittest we do create/destroy multiple Interpreter's.
	// Another option is to not call Tcl_Finalize(), but that leaves some
	// memory allocated, and makes memory-leak checkers report more errors.
	static bool scheduled = false;
	if (!scheduled) {
		scheduled = true;
		atexit(Tcl_Finalize);
	}
}

int Interpreter::outputProc(ClientData clientData, const char* buf,
                 int toWrite, int* /*errorCodePtr*/)
{
	try {
		auto* output = static_cast<Interpreter*>(clientData)->output;
		std::string_view text(buf, toWrite);
		if (!text.empty() && output) {
			output->output(text);
		}
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
	}
	return toWrite;
}

bool Interpreter::hasCommand(zstring_view name) const
{
	// Note: these are not only the commands registered via
	// registerCommand(), but all commands know to this Tcl-interpreter.
	return Tcl_FindCommand(interp, name.c_str(), nullptr, 0);
}

void Interpreter::registerCommand(zstring_view name, Command& command)
{
	auto token = Tcl_CreateObjCommand(
		interp, name.c_str(), commandProc,
		static_cast<ClientData>(&command), nullptr);
	command.setToken(token);
}

void Interpreter::unregisterCommand(Command& command)
{
	Tcl_DeleteCommandFromToken(interp, static_cast<Tcl_Command>(command.getToken()));
}

int Interpreter::commandProc(ClientData clientData, Tcl_Interp* interp,
                             int objc, Tcl_Obj* const* objv)
{
	try {
		auto& command = *static_cast<Command*>(clientData);
		std::span<const TclObject> tokens(
			std::bit_cast<TclObject*>(const_cast<Tcl_Obj**>(objv)),
			objc);
		int res = TCL_OK;
		TclObject result;
		try {
			if (!command.isAllowedInEmptyMachine()) {
				if (auto* controller =
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
			result = e.getMessage();
			res = TCL_ERROR;
		}
		Tcl_SetObjResult(interp, result.getTclObject());
		return res;
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
	}
}

// Returns
//   - build-in Tcl commands
//   - openmsx commands
//   - user-defined procs
TclObject Interpreter::getCommandNames()
{
	return execute("openmsx::all_command_names");
}

bool Interpreter::isComplete(zstring_view command) const
{
	return Tcl_CommandComplete(command.c_str()) != 0;
}

TclObject Interpreter::execute(zstring_view command)
{
	if (Tcl_Eval(interp, command.c_str()) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return TclObject(Tcl_GetObjResult(interp));
}

TclObject Interpreter::executeFile(zstring_view filename)
{
	if (Tcl_EvalFile(interp, filename.c_str()) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return TclObject(Tcl_GetObjResult(interp));
}

static void setVar(Tcl_Interp* interp, const TclObject& name, const TclObject& value)
{
	if (!Tcl_ObjSetVar2(interp, name.getTclObjectNonConst(), nullptr,
		            value.getTclObjectNonConst(),
		            TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) {
		// might contain error message of a trace proc
		std::cerr << Tcl_GetStringResult(interp) << '\n';
	}
}
static Tcl_Obj* getVar(Tcl_Interp* interp, const TclObject& name)
{
	return Tcl_ObjGetVar2(interp, name.getTclObjectNonConst(), nullptr,
	                      TCL_GLOBAL_ONLY);
}

void Interpreter::setVariable(const TclObject& name, const TclObject& value)
{
	if (!Tcl_ObjSetVar2(interp, name.getTclObjectNonConst(), nullptr,
		            value.getTclObjectNonConst(),
		            TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
}

void Interpreter::setVariable(const TclObject& arrayName, const TclObject& arrayIndex, const TclObject& value)
{
	if (!Tcl_ObjSetVar2(interp, arrayName.getTclObjectNonConst(), arrayIndex.getTclObjectNonConst(),
		            value.getTclObjectNonConst(),
		            TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
}

void Interpreter::unsetVariable(const char* name)
{
	Tcl_UnsetVar(interp, name, TCL_GLOBAL_ONLY);
}

static TclObject getSafeValue(BaseSetting& setting)
{
	// TODO use c++23 std::optional<T>::or_else()
	if (auto val = setting.getOptionalValue()) {
		return *val;
	}
	return TclObject(0); // 'safe' value, see comment in registerSetting()
}
void Interpreter::registerSetting(BaseSetting& variable)
{
	const auto& name = variable.getFullNameObj();
	if (Tcl_Obj* tclVarValue = getVar(interp, name)) {
		// Tcl var already existed, use this value
		try {
			variable.setValueDirect(TclObject(tclVarValue));
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
		setVariable(name, getSafeValue(variable));
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
	// Before changing the 'safe-value' (see getSafeValue()) to '0',
	// this gave errors because the load_icons script didn't expect to see
	// 'proxy' (the old 'safe-value') as value.
	//
	// The current solution (choosing '0' as safe value) is not ideal, but
	// good enough for now.
	//
	// A possible better solution is to move Tcl_TraceVar() before
	// setVariable(), I did an initial attempt but there were some
	// problems. TODO investigate this further.

	uintptr_t traceID = traceCount++;
	traces.emplace_back(Trace{traceID, &variable}); // still in sorted order
	Tcl_TraceVar(interp, name.getString().data(), // 0-terminated
	             TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	             traceProc, std::bit_cast<ClientData>(traceID));
}

void Interpreter::unregisterSetting(BaseSetting& variable)
{
	auto it = rfind_unguarded(traces, &variable, &Trace::setting);
	uintptr_t traceID = it->id;
	traces.erase(it);

	const char* name = variable.getFullName().data(); // 0-terminated
	Tcl_UntraceVar(interp, name,
	               TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	               traceProc, std::bit_cast<ClientData>(traceID));
	unsetVariable(name);
}

static BaseSetting* getTraceSetting(uintptr_t traceID)
{
	auto t = binary_find(traces, traceID, {}, &Trace::id);
	return t ? t->setting : nullptr;
}

#ifndef NDEBUG
static std::string_view removeColonColon(std::string_view s)
{
	if (s.starts_with("::")) s.remove_prefix(2);
	return s;
}
#endif

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
		// a setting we unset the corresponding Tcl variable (see
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

		auto traceID = std::bit_cast<uintptr_t>(clientData);
		auto* variable = getTraceSetting(traceID);
		if (!variable) return nullptr;

		const TclObject& part1Obj = variable->getFullNameObj();
		assert(removeColonColon(part1) == removeColonColon(part1Obj.getString()));

		static std::string static_string;
		if (flags & TCL_TRACE_READS) {
			try {
				setVar(interp, part1Obj, variable->getValue());
			} catch (MSXException& e) {
				static_string = std::move(e).getMessage();
				return const_cast<char*>(static_string.c_str());
			}
		}
		if (flags & TCL_TRACE_WRITES) {
			try {
				Tcl_Obj* v = getVar(interp, part1Obj);
				TclObject newValue(v ? v : Tcl_NewObj());
				variable->setValueDirect(newValue);
				const TclObject& newValue2 = variable->getValue();
				if (newValue != newValue2) {
					setVar(interp, part1Obj, newValue2);
				}
			} catch (MSXException& e) {
				setVar(interp, part1Obj, getSafeValue(*variable));
				static_string = std::move(e).getMessage();
				return const_cast<char*>(static_string.c_str());
			}
		}
		if (flags & TCL_TRACE_UNSETS) {
			try {
				// note we cannot use restoreDefault(), because
				// that goes via Tcl and the Tcl variable
				// doesn't exist at this point
				variable->setValueDirect(TclObject(
					variable->getDefaultValue()));
			} catch (MSXException&) {
				// for some reason default value is not valid ATM,
				// keep current value (happened for videosource
				// setting before turning on (set power on) the
				// MSX machine)
			}
			setVar(interp, part1Obj, getSafeValue(*variable));
			Tcl_TraceVar(interp, part1, TCL_TRACE_READS |
			                TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
			             traceProc,
			             std::bit_cast<ClientData>(traceID));
		}
	} catch (...) {
		UNREACHABLE; // we cannot let exceptions pass through Tcl
	}
	return nullptr;
}

void Interpreter::createNamespace(const std::string& name)
{
	execute(tmpStrCat("namespace eval ", name, " {}"));
}

void Interpreter::deleteNamespace(const std::string& name)
{
	execute(tmpStrCat("namespace delete ", name));
}

void Interpreter::poll() const
{
	//Tcl_ServiceAll();
	Tcl_DoOneEvent(TCL_DONT_WAIT);
}

TclParser Interpreter::parse(std::string_view command)
{
	return {interp, command};
}

bool Interpreter::validCommand(std::string_view command)
{
	Tcl_Parse parseInfo;
	int result = Tcl_ParseCommand(interp, command.data(), narrow<int>(command.size()), 0, &parseInfo);
	Tcl_FreeParse(&parseInfo);
	return result == TCL_OK;
}

bool Interpreter::validExpression(std::string_view expression)
{
	Tcl_Parse parseInfo;
	int result = Tcl_ParseExpr(interp, expression.data(), narrow<int>(expression.size()), &parseInfo);
	Tcl_FreeParse(&parseInfo);
	return result == TCL_OK;
}

void Interpreter::wrongNumArgs(unsigned argc, std::span<const TclObject> tokens, const char* message)
{
	assert(argc <= tokens.size());
	Tcl_WrongNumArgs(interp, narrow<int>(argc), std::bit_cast<Tcl_Obj* const*>(tokens.data()), message);
	// not efficient, but anyway on an error path
	throw CommandException(Tcl_GetStringResult(interp));
}

} // namespace openmsx
