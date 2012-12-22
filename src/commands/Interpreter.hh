// $Id$

#ifndef INTERPRETER_HH
#define INTERPRETER_HH

#include "TclParser.hh"
#include "EventListener.hh"
#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include "vla.hh"
#include <vector>
#include <tcl.h>

namespace openmsx {

class EventDistributor;
class Command;
class Setting;
class InterpreterOutput;
class TclObject;

class Interpreter : private EventListener, private noncopyable
{
public:
	explicit Interpreter(EventDistributor& eventDistributor);
	~Interpreter();

	void setOutput(InterpreterOutput* output);

	void init(const char* programName);
	void registerCommand(const std::string& name, Command& command);
	void unregisterCommand(string_ref name, Command& command);
	std::vector<std::string> getCommandNames();
	bool isComplete(const std::string& command) const;
	std::string execute(const std::string& command);
	std::string executeFile(const std::string& filename);

	void setVariable(const std::string& name, const std::string& value);
	void unsetVariable(const std::string& name);
	const char* getVariable(const std::string& name) const;
	void registerSetting(Setting& variable, const std::string& name);
	void unregisterSetting(Setting& variable, const std::string& name);

	/** Create the global namespace with given name.
	  * @param name Name of the namespace, should not include '::' prefix.
	  */
	void createNamespace(const std::string& name);

	/** Delete the global namespace with given name.
	  * @param name Name of the namespace, should not include '::' prefix.
	  */
	void deleteNamespace(const std::string& name);

	std::vector<std::string> splitList(const std::string& list);
	static std::vector<std::string> splitList(
		const std::string& list, Tcl_Interp* interp);

	TclParser parse(string_ref command);

private:
	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	void poll();

	static int outputProc(ClientData clientData, const char* buf,
	        int toWrite, int* errorCodePtr);
	static int commandProc(ClientData clientData, Tcl_Interp* interp,
	                       int objc, Tcl_Obj* const objv[]);
	static char* traceProc(ClientData clientData, Tcl_Interp* interp,
	                       const char* part1, const char* part2, int flags);

	EventDistributor& eventDistributor;

	static Tcl_ChannelType channelType;
	Tcl_Interp* interp;
	StringMap<Tcl_Command> commandTokenMap;
	InterpreterOutput* output;

	friend class TclObject;
};

} // namespace openmsx

#endif
