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

void TclObject::checkExpression() const
{
	string tmp = getString();
	parse(tmp.data(), tmp.size(), true);
}

void TclObject::parse(const char* str, int len, bool expression) const
{
	Tcl_Parse info;
	if (expression ?
	    Tcl_ParseExpr(interp, str, len, &info) :
	    Tcl_ParseCommand(interp, str, len, 1, &info) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	struct Cleanup {
		~Cleanup() { Tcl_FreeParse(p); }
		Tcl_Parse* p;
	} cleanup = { &info };
	if (&cleanup); // avoid warning
	
	if (!expression && (info.tokenPtr[0].type == TCL_TOKEN_SIMPLE_WORD)) {
		// simple command name
		Tcl_CmdInfo cmdinfo;
		Tcl_Token& token2 = info.tokenPtr[1];
		string procname(token2.start, token2.size);
		if (!Tcl_GetCommandInfo(interp, procname.c_str(), &cmdinfo)) {
			throw CommandException("invalid command name: \"" +
			                       procname + '\"');
		}
	}
	for (int i = 0; i < info.numTokens; ++i) {
		Tcl_Token& token = info.tokenPtr[i];
		if (token.type == TCL_TOKEN_COMMAND) {
			parse(token.start + 1, token.size - 2, false);
		} else if ((token.type == TCL_TOKEN_VARIABLE) &&
				(token.numComponents == 1)) {
			// simple variable (no array)
			Tcl_Token& token2 = info.tokenPtr[i + 1];
			string varname(token2.start, token2.size);
			if (!Tcl_GetVar2Ex(interp, varname.c_str(), NULL,
			                   TCL_LEAVE_ERR_MSG)) {
				throw CommandException(Tcl_GetStringResult(interp));
			}
		}
	}
}

} // namespace openmsx

