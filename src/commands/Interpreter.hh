#ifndef INTERPRETER_HH
#define INTERPRETER_HH

#include "TclParser.hh"
#include "TclObject.hh"
#include "zstring_view.hh"
#include <tcl.h>
#include <span>
#include <string_view>
#include <string>

namespace openmsx {

class Command;
class BaseSetting;
class InterpreterOutput;

class Interpreter
{
public:
	Interpreter();
	Interpreter(const Interpreter&) = delete;
	Interpreter(Interpreter&&) = delete;
	Interpreter& operator=(const Interpreter&) = delete;
	Interpreter& operator=(Interpreter&&) = delete;
	~Interpreter();

	void setOutput(InterpreterOutput* output_) { output = output_; }

	void init(const char* programName) const;
	[[nodiscard]] bool hasCommand(zstring_view name) const;
	void registerCommand(zstring_view name, Command& command);
	void unregisterCommand(Command& command);
	[[nodiscard]] TclObject getCommandNames();
	[[nodiscard]] bool isComplete(zstring_view command) const;
	TclObject execute(zstring_view command);
	TclObject executeFile(zstring_view filename);

	void setVariable(const TclObject& name, const TclObject& value);
	void setVariable(const TclObject& arrayName, const TclObject& arrayIndex, const TclObject& value);
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

	[[nodiscard]] TclParser parse(std::string_view command);
	[[nodiscard]] bool validExpression(std::string_view expression);
	[[nodiscard]] std::string parseCommandError(std::string_view command);
	[[nodiscard]] std::string parseExpressionError(std::string_view expression);

	void poll() const;

	[[noreturn]] void wrongNumArgs(unsigned argc, std::span<const TclObject> tokens, const char* message);

private:
	static int outputProc(ClientData clientData, const char* buf,
	                      int toWrite, int* errorCodePtr);
	static int commandProc(ClientData clientData, Tcl_Interp* interp,
	                       int objc, Tcl_Obj* const* objv);
	static char* traceProc(ClientData clientData, Tcl_Interp* interp,
	                       const char* part1, const char* part2, int flags);

	static Tcl_ChannelType channelType;
	Tcl_Interp* interp;
	InterpreterOutput* output;

	friend class TclObject;
};

} // namespace openmsx

#endif
