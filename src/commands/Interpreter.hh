// $Id$

#ifndef __INTERPRETER_HH__
#define __INTERPRETER_HH__

#include <set>
#include <string>
#include <tcl.h>
#include "Command.hh"

using std::set;
using std::string;

namespace openmsx {

class Setting;

class Interpreter
{
public:
	static Interpreter& instance();
	
	void init(const char* programName);
	void registerCommand(const string& name, Command& command);
	void unregisterCommand(const string& name, Command& command);
	void getCommandNames(set<string>& result);
	bool isComplete(const string& command) const;
	string execute(const string& command);
	string executeFile(const string& filename);
	
	void setVariable(const string& name, const string& value);
	void unsetVariable(const string& name);
	string getVariable(const string& name) const;
	void registerSetting(Setting& variable);
	void unregisterSetting(Setting& variable);

	void splitList(const string& list, vector<string>& result);

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
