// $Id$

#ifndef COMMANDARGUMENT_HH
#define COMMANDARGUMENT_HH

#include <string>
#include "openmsx.hh"

class Tcl_Interp;
class Tcl_Obj;

namespace openmsx {

class CommandArgument
{
public:
	CommandArgument(Tcl_Interp* interp, Tcl_Obj* object);
	
	void setString(const std::string& value);
	void setInt(int value);
	void setDouble(double value);
	void setBinary(byte* buf, unsigned length);
	void addListElement(const std::string& element);
	
	std::string getString() const;
	int getInt() const;
	double getDouble() const;
	const byte* getBinary(unsigned& length) const;

private:
	Tcl_Interp* interp;
	Tcl_Obj* obj;
};

class CommandResult : public CommandArgument
{
public:
	CommandResult(Tcl_Interp* interp);
};

} // namespace openmsx

#endif
