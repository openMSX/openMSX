// $Id$

#ifndef INTERPRETER_HH
#define INTERPRETER_HH

#include <set>
#include <string>
#include <tcl.h>

namespace openmsx {

class Command;
class Setting;

class Interpreter
{
public:
	static Interpreter& instance();
	
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
	Interpreter();
	~Interpreter();
	
	static int outputProc(ClientData clientData, const char* buf,
	        int toWrite, int* errorCodePtr);
	static int commandProc(ClientData clientData, Tcl_Interp* interp,
                               int objc, Tcl_Obj* const objv[]);
	static char* traceProc(ClientData clientData, Tcl_Interp* interp,
                const char* part1, const char* part2, int flags);
	
	static Tcl_ChannelType channelType;
	Tcl_Interp* interp;
};

} // namespace openmsx

#endif
