// $Id$

#include "TclObject.hh"
#include "Interpreter.hh"
#include "CommandException.hh"
#include <cassert>
#include <tcl.h>

using std::string;

namespace openmsx {

// class TclObject

TclObject::TclObject(Tcl_Interp* interp_, Tcl_Obj* obj_)
	: interp(interp_), obj(obj_), owned(false)
{
}

TclObject::TclObject(Tcl_Interp* interp_, string_ref value)
	: interp(interp_)
{
	init(Tcl_NewStringObj(value.data(), int(value.size())));
}

TclObject::TclObject(string_ref value)
	: interp(0)
{
	init(Tcl_NewStringObj(value.data(), int(value.size())));
}

TclObject::TclObject(Tcl_Interp* interp_)
	: interp(interp_)
{
	init(Tcl_NewObj());
}

TclObject::TclObject(Interpreter& interp_)
	: interp(interp_.interp)
{
	init(Tcl_NewObj());
}

TclObject::TclObject(const TclObject& object)
	: interp(object.interp)
{
	init(object.obj);
}

TclObject::TclObject()
	: interp(0)
{
	init(Tcl_NewObj());
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

TclObject& TclObject::operator=(const TclObject& other)
{
	if (&other != this) {
		if (owned) {
			Tcl_DecrRefCount(obj);
		}
		interp = other.interp;
		init(other.obj);
	}
	return *this;
}

Tcl_Interp* TclObject::getInterpreter() const
{
	return interp;
}

Tcl_Obj* TclObject::getTclObject()
{
	return obj;
}

void TclObject::unshare()
{
	if (Tcl_IsShared(obj)) {
		assert(owned);
		Tcl_DecrRefCount(obj);
		obj = Tcl_DuplicateObj(obj);
		Tcl_IncrRefCount(obj);
	}
	assert(!Tcl_IsShared(obj));
}

void TclObject::throwException() const
{
	string_ref message = interp ? Tcl_GetStringResult(interp)
	                        : "TclObject error";
	throw CommandException(message);
}

void TclObject::setString(string_ref value)
{
	unshare();
	Tcl_SetStringObj(obj, value.data(), int(value.size()));
}

void TclObject::setInt(int value)
{
	unshare();
	Tcl_SetIntObj(obj, value);
}

void TclObject::setBoolean(bool value)
{
	unshare();
	Tcl_SetBooleanObj(obj, value);
}

void TclObject::setDouble(double value)
{
	unshare();
	Tcl_SetDoubleObj(obj, value);
}

void TclObject::setBinary(byte* buf, unsigned length)
{
	unshare();
	Tcl_SetByteArrayObj(obj, buf, length);
}

void TclObject::addListElement(string_ref element)
{
	addListElement(Tcl_NewStringObj(element.data(), int(element.size())));
}

void TclObject::addListElement(int value)
{
	addListElement(Tcl_NewIntObj(value));
}

void TclObject::addListElement(double value)
{
	addListElement(Tcl_NewDoubleObj(value));
}

void TclObject::addListElement(const TclObject& element)
{
	addListElement(element.obj);
}

void TclObject::addListElement(Tcl_Obj* element)
{
	unshare();
	if (Tcl_ListObjAppendElement(interp, obj, element) != TCL_OK) {
		throwException();
	}
}

int TclObject::getInt() const
{
	int result;
	if (Tcl_GetIntFromObj(interp, obj, &result) != TCL_OK) {
		throwException();
	}
	return result;
}

bool TclObject::getBoolean() const
{
	int result;
	if (Tcl_GetBooleanFromObj(interp, obj, &result) != TCL_OK) {
		throwException();
	}
	return result != 0;
}

double TclObject::getDouble() const
{
	double result;
	if (Tcl_GetDoubleFromObj(interp, obj, &result) != TCL_OK) {
		throwException();
	}
	return result;
}

string_ref TclObject::getString() const
{
	int length;
	char* buf = Tcl_GetStringFromObj(obj, &length);
	return string_ref(buf, length);
}

const byte* TclObject::getBinary(unsigned& length) const
{
	return static_cast<const byte*>(Tcl_GetByteArrayFromObj(
		obj, reinterpret_cast<int*>(&length)));
}

unsigned TclObject::getListLength() const
{
	int result;
	if (Tcl_ListObjLength(interp, obj, &result) != TCL_OK) {
		throwException();
	}
	return result;
}

TclObject TclObject::getListIndex(unsigned index) const
{
	Tcl_Obj* element;
	if (Tcl_ListObjIndex(interp, obj, index, &element) != TCL_OK) {
		throwException();
	}
	return element ? TclObject(interp, element)
	               : TclObject(interp);
}

bool TclObject::evalBool() const
{
	int result;
	if (Tcl_ExprBooleanObj(interp, obj, &result) != TCL_OK) {
		throwException();
	}
	return result != 0;
}

void TclObject::checkExpression() const
{
	string_ref tmp = getString();
	parse(tmp.data(), int(tmp.size()), true);
}

void TclObject::checkCommand() const
{
	string_ref tmp = getString();
	parse(tmp.data(), int(tmp.size()), false);
}

void TclObject::parse(const char* str, int len, bool expression) const
{
	assert(interp);
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
	(void)cleanup;

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

string TclObject::executeCommand(bool compile)
{
	assert(interp);
	int flags = compile ? 0 : TCL_EVAL_DIRECT;
	int success = Tcl_EvalObjEx(interp, obj, flags);
	string result =  Tcl_GetStringResult(interp);
	if (success != TCL_OK) {
		throw CommandException(result);
	}
	return result;
}


} // namespace openmsx

