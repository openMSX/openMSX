// $Id$

#include <tcl.h>
#include "CommandArgument.hh"
#include "CommandException.hh"

using std::string;

namespace openmsx {

// class CommandArgument

CommandArgument::CommandArgument(Tcl_Interp* interp_, Tcl_Obj* obj_)
	: interp(interp_), obj(obj_)
{
}

void CommandArgument::setString(const string& value)
{
	Tcl_SetStringObj(obj, value.c_str(), value.length());
}

void CommandArgument::setInt(int value)
{
	Tcl_SetIntObj(obj, value);
}

void CommandArgument::setDouble(double value)
{
	Tcl_SetDoubleObj(obj, value);
}

void CommandArgument::setBinary(byte* buf, unsigned length)
{
	Tcl_SetByteArrayObj(obj, buf, length);
}

void CommandArgument::addListElement(const string& element)
{
	Tcl_AppendElement(interp, element.c_str());
}

int CommandArgument::getInt() const
{
	int result;
	if (Tcl_GetIntFromObj(interp, obj, &result) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return result;
}

double CommandArgument::getDouble() const
{
	double result;
	if (Tcl_GetDoubleFromObj(interp, obj, &result) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return result;
}

string CommandArgument::getString() const
{
	int length;
	char* buf = Tcl_GetStringFromObj(obj, &length);
	return string(buf, length);
}

const byte* CommandArgument::getBinary(unsigned& length) const
{
	return static_cast<const byte*>(Tcl_GetByteArrayFromObj(
		obj, reinterpret_cast<int*>(&length)));
}


// class CommandResult

CommandResult::CommandResult(Tcl_Interp* interp_)
	: CommandArgument(interp_, Tcl_GetObjResult(interp_))
{
}

} // namespace openmsx

