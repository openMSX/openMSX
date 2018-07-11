#ifndef INTERPRETER_HH
#define INTERPRETER_HH

#include "TclParser.hh"
#include "TclObject.hh"
#include "string_view.hh"
#include <vector>
#include <tcl.h>

namespace openmsx {

class EventDistributor;
class Command;
class BaseSetting;
class InterpreterOutput;

class Interpreter
{
public:
	Interpreter(const Interpreter&) = delete;
	Interpreter& operator=(const Interpreter&) = delete;

	explicit Interpreter(EventDistributor& eventDistributor);
	~Interpreter();

	void setOutput(InterpreterOutput* output_) { output = output_; }

	void init(const char* programName);
	void registerCommand(const std::string& name, Command& command);
	void unregisterCommand(Command& command);
	TclObject getCommandNames();
	bool isComplete(const std::string& command) const;
	TclObject execute(const std::string& command);
	TclObject executeFile(const std::string& filename);

	void setVariable(const TclObject& name, const TclObject& value);
	void unsetVariable(const char* name);
	void registerSetting(BaseSetting& variable);
	void unregisterSetting(BaseSetting& variable);

	/** Create the global namespace with given name.
	  * @param name Name of the namespace, should not include '::' prefix.
	  */
	void createNamespace(const std::string& name);

	/** Delete the global namespace with given name.
	  * @param name Name of the namespace, should not include '::' prefix.
	  */
	void deleteNamespace(const std::string& name);

	TclParser parse(string_view command);

	void poll();

private:
	static int outputProc(ClientData clientData, const char* buf,
	        int toWrite, int* errorCodePtr);
	static int commandProc(ClientData clientData, Tcl_Interp* interp,
	                       int objc, Tcl_Obj* const objv[]);
	static char* traceProc(ClientData clientData, Tcl_Interp* interp,
	                       const char* part1, const char* part2, int flags);

	EventDistributor& eventDistributor;

	static Tcl_ChannelType channelType;
	Tcl_Interp* interp;
	InterpreterOutput* output;

	friend class TclObject;
};

} // namespace openmsx

#endif
