// $Id$

#include "TCLCommandResult.hh"
#include <tcl.h>

namespace openmsx {

TCLCommandResult::TCLCommandResult(Tcl_Interp* interp_)
	: interp(interp_)
{
}

void TCLCommandResult::setString(const string& value)
{
	Tcl_Obj* obj = Tcl_GetObjResult(interp);
	Tcl_SetStringObj(obj, value.c_str(), value.length());
}

void TCLCommandResult::setInt(int value)
{
	Tcl_Obj* obj = Tcl_GetObjResult(interp);
	Tcl_SetIntObj(obj, value);
}

void TCLCommandResult::setDouble(double value)
{
	Tcl_Obj* obj = Tcl_GetObjResult(interp);
	Tcl_SetDoubleObj(obj, value);
}

void TCLCommandResult::setBinary(byte* buf, unsigned length)
{
	Tcl_Obj* obj = Tcl_GetObjResult(interp);
	Tcl_SetByteArrayObj(obj, buf, length);
}

void TCLCommandResult::addListElement(const string& element)
{
	Tcl_AppendElement(interp, element.c_str());
}

} // namespace openmsx

