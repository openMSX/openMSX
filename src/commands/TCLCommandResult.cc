//$ Id: $

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

TCLCommandResult::TCLCommandResult(Tcl_Obj* obj_)
	: obj(obj_)
{
}

void TCLCommandResult::setString(const string& value)
{
	Tcl_SetStringObj(obj, value.c_str(), value.length());
}

void TCLCommandResult::setInt(int value)
{
	Tcl_SetIntObj(obj, value);
}

void TCLCommandResult::setBinary(byte* buf, unsigned length)
{
	Tcl_SetByteArrayObj(obj, buf, length);
}

} // namespace openmsx

