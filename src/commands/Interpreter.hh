// $Id$

#ifndef INTERPRETER_HH
#define INTERPRETER_HH

#include "EventListener.hh"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <tcl.h>

namespace openmsx {

class EventDistributor;
class Command;
class Setting;
class InterpreterOutput;
class TclObject;

class Interpreter : private EventListener
{
public:
	explicit Interpreter(EventDistributor& eventDistributor);
	~Interpreter();

	void setOutput(InterpreterOutput* output);

	void init(const char* programName);
	void registerCommand(const std::string& name, Command& command);
	void unregisterCommand(const std::string& name, Command& command);
	void getCommandNames(std::set<std::string>& result);
	bool isComplete(const std::string& command) const;
	std::string execute(const std::string& command);
	std::string executeFile(const std::string& filename);

	void setVariable(const std::string& name, const std::string& value);
	void unsetVariable(const std::string& name);
	const char* getVariable(const std::string& name) const;
	void registerSetting(Setting& variable);
	void unregisterSetting(Setting& variable);

	void splitList(const std::string& list,
	               std::vector<std::string>& result);


private:
	// EventListener
	virtual void signalEvent(const Event& event);

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
	std::map<std::string, Tcl_Command> commandTokenMap;
	InterpreterOutput* output;

	friend class TclObject;
};

} // namespace openmsx

#endif
