// $Id$

#ifndef __TCLINTERP_HH__
#define __TCLINTERP_HH__

#include <tcl.h>
#include <SettingNode.hh>
#include <Command.hh>

namespace openmsx {

class TCLInterp
{
public:
	static TCLInterp& instance();

	void registerCommand(const string& name, Command& command);
	void unregisterCommand(const string& name, Command& command);
	bool isComplete(const string& command) const;
	string execute(const string& command) throw(CommandException);
	string executeFile(const string& filename) throw(CommandException);
	
	void setVariable(const string& name, const string& value);
	void unsetVariable(const string& name);
	string getVariable(const string& name) const;
	void registerSetting(SettingLeafNode& variable);
	void unregisterSetting(SettingLeafNode& variable);

private:
	TCLInterp();
	~TCLInterp();

	static int outputProc(ClientData clientData, const char* buf,
	        int toWrite, int* errorCodePtr);
	static int commandProc(ClientData clientData, Tcl_Interp* interp,
                               int objc, Tcl_Obj* const objv[]);
	static char* traceProc(ClientData clientData, Tcl_Interp* interp,
                const char* part1, const char* part2, int flags);
	
	Tcl_Interp* interp;
};

} // namespace openmsx

#endif
