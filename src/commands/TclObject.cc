// $Id$

#include <tcl.h>
#include "TclObject.hh"
#include "CommandException.hh"

using std::string;

namespace openmsx {

// class TclObject

TclObject::TclObject(Tcl_Interp* interp_, Tcl_Obj* obj_)
	: interp(interp_), obj(obj_), owned(false)
{
}

TclObject::TclObject(Tcl_Interp* interp_, const string& value)
	: interp(interp_)
{
	init(Tcl_NewStringObj(value.data(), value.size()));
}

TclObject::TclObject(const TclObject& object)
	: interp(object.interp)
{
	init(Tcl_DuplicateObj(object.obj));
}

void TclObject::init(Tcl_Obj* obj_)
{
	obj = obj_;
	owned = true;
	Tcl_IncrRefCount(obj);
}

TclObject::~TclObject()
{
	if (owned) {
		Tcl_DecrRefCount(obj);
	}
}

void TclObject::setString(const string& value)
{
	Tcl_SetStringObj(obj, value.c_str(), value.length());
}

void TclObject::setInt(int value)
{
	Tcl_SetIntObj(obj, value);
}

void TclObject::setDouble(double value)
{
	Tcl_SetDoubleObj(obj, value);
}

void TclObject::setBinary(byte* buf, unsigned length)
{
	Tcl_SetByteArrayObj(obj, buf, length);
}

void TclObject::addListElement(const string& element)
{
	Tcl_AppendElement(interp, element.c_str());
}

int TclObject::getInt() const
{
	int result;
	if (Tcl_GetIntFromObj(interp, obj, &result) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return result;
}

double TclObject::getDouble() const
{
	double result;
	if (Tcl_GetDoubleFromObj(interp, obj, &result) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return result;
}

string TclObject::getString() const
{
	int length;
	char* buf = Tcl_GetStringFromObj(obj, &length);
	return string(buf, length);
}

const byte* TclObject::getBinary(unsigned& length) const
{
	return static_cast<const byte*>(Tcl_GetByteArrayFromObj(
		obj, reinterpret_cast<int*>(&length)));
}

bool TclObject::evalBool() const
{
	int result;
	if (Tcl_ExprBooleanObj(interp, obj, &result) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return result;
}

} // namespace openmsx

