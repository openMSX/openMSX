// $Id$

#include "config.h"
#if defined(HAVE_TCL_H)
#include <tcl.h>
#elif defined(HAVE_TCL8_4_TCL_H)
#include <tcl8.4/tcl.h>
#elif defined(HAVE_TCL8_3_TCL_H)
#include <tcl8.3/tcl.h>
#endif

#include "TCLCommandResult.hh"

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

