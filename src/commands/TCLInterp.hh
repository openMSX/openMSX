// $Id$

#ifndef __TCLINTERP_HH__
#define __TCLINTERP_HH__

#include "config.h"
#if	defined(HAVE_TCL_H)
#include <tcl.h>
#elif	defined(HAVE_TCL8_4_TCL_H)
#include <tcl8.4/tcl.h>
#elif	defined(HAVE_TCL8_3_TCL_H)
#include <tcl8.3/tcl.h>
#endif

#include <Interpreter.hh>

namespace openmsx {

class TCLInterp : public Interpreter
{
public:
	TCLInterp();
	virtual ~TCLInterp();

	virtual void init(const char* programName);
	virtual void registerCommand(const string& name, Command& command);
	virtual void unregisterCommand(const string& name, Command& command);
	virtual void getCommandNames(set<string>& result);
	virtual bool isComplete(const string& command) const;
	virtual string execute(const string& command) throw(CommandException);
	virtual string executeFile(const string& filename) throw(CommandException);
	
	virtual void setVariable(const string& name, const string& value);
	virtual void unsetVariable(const string& name);
	virtual string getVariable(const string& name) const;
	virtual void registerSetting(SettingLeafNode& variable);
	virtual void unregisterSetting(SettingLeafNode& variable);

private:
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
