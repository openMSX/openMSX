// $Id$

#ifndef __TCLCOMMANDRESULT_HH__
#define __TCLCOMMANDRESULT_HH__

#include "CommandResult.hh"

class Tcl_Obj;

namespace openmsx {

class TCLCommandResult : public CommandResult
{
public:
	TCLCommandResult(Tcl_Obj* obj);
	
	virtual void setString(const string& value);
	virtual void setInt(int value);
	virtual void setBinary(byte* buf, unsigned length);

private:
	Tcl_Obj* obj;
};

} // namespace openmsx

#endif
