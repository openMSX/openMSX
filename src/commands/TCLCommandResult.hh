// $Id$

#ifndef __TCLCOMMANDRESULT_HH__
#define __TCLCOMMANDRESULT_HH__

#include "CommandResult.hh"

class Tcl_Interp;

namespace openmsx {

class TCLCommandResult : public CommandResult
{
public:
	TCLCommandResult(Tcl_Interp* interp);
	
	virtual void setString(const string& value);
	virtual void setInt(int value);
	virtual void setDouble(double value);
	virtual void setBinary(byte* buf, unsigned length);
	virtual void addListElement(const string& element);

private:
	Tcl_Interp* interp;
};

} // namespace openmsx

#endif
