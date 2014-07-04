#include "TclObject.hh"
#include "Interpreter.hh"
#include "CommandException.hh"
#include <cassert>
#include <tcl.h>

using std::string;

namespace openmsx {

// class TclObject

TclObject::TclObject(Tcl_Interp* interp_, Tcl_Obj* obj_)
	: interp(interp_)
{
	init(obj_);
}

TclObject::TclObject(Tcl_Interp* interp_, string_ref value)
	: interp(interp_)
{
	init(Tcl_NewStringObj(value.data(), int(value.size())));
}

TclObject::TclObject(Interpreter& interp_, string_ref value)
	: interp(interp_.interp)
{
	init(Tcl_NewStringObj(value.data(), int(value.size())));
}

TclObject::TclObject(string_ref value)
	: interp(nullptr)
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
	: interp(nullptr)
{
	init(Tcl_NewObj());
}

void TclObject::init(Tcl_Obj* obj_)
{
	obj = obj_;
	Tcl_IncrRefCount(obj);
}

TclObject::~TclObject()
{
	Tcl_DecrRefCount(obj);
}

TclObject& TclObject::operator=(const TclObject& other)
{
	if (&other != this) {
		Tcl_DecrRefCount(obj);
		interp = other.interp;
		init(other.obj);
	}
	return *this;
}

Tcl_Obj* TclObject::getTclObject()
{
	return obj;
}

static void throwException(Tcl_Interp* interp)
{
	string_ref message = interp ? Tcl_GetStringResult(interp)
	                            : "TclObject error";
	throw CommandException(message);
}

void TclObject::setString(string_ref value)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_NewStringObj(value.data(), int(value.size()));
		Tcl_IncrRefCount(obj);
	} else {
		Tcl_SetStringObj(obj, value.data(), int(value.size()));
	}
}

void TclObject::setInt(int value)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_NewIntObj(value);
		Tcl_IncrRefCount(obj);
	} else {
		Tcl_SetIntObj(obj, value);
	}
}

void TclObject::setBoolean(bool value)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_NewBooleanObj(value);
		Tcl_IncrRefCount(obj);
	} else {
		Tcl_SetBooleanObj(obj, value);
	}
}

void TclObject::setDouble(double value)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_NewDoubleObj(value);
		Tcl_IncrRefCount(obj);
	} else {
		Tcl_SetDoubleObj(obj, value);
	}
}

void TclObject::setBinary(byte* buf, unsigned length)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_NewByteArrayObj(buf, length);
		Tcl_IncrRefCount(obj);
	} else {
		Tcl_SetByteArrayObj(obj, buf, length);
	}
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
	// Although it's theoretically possible that Tcl_ListObjAppendElement()
	// returns an error (e.g. adding an element to a string containing
	// unbalanced quotes), this rarely occurs in our context. So we don't
	// require passing an Interpreter parameter in all addListElement()
	// functions. And in the very unlikely case that it does happen the
	// only problem is that the error message is less descriptive than it
	// could be.
	Tcl_Interp* interp = nullptr;
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_DuplicateObj(obj);
		Tcl_IncrRefCount(obj);
	}
	if (Tcl_ListObjAppendElement(interp, obj, element) != TCL_OK) {
		throwException(interp);
	}
}

int TclObject::getInt(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	int result;
	if (Tcl_GetIntFromObj(interp, obj, &result) != TCL_OK) {
		throwException(interp);
	}
	return result;
}

bool TclObject::getBoolean(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	int result;
	if (Tcl_GetBooleanFromObj(interp, obj, &result) != TCL_OK) {
		throwException(interp);
	}
	return result != 0;
}

double TclObject::getDouble(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	double result;
	if (Tcl_GetDoubleFromObj(interp, obj, &result) != TCL_OK) {
		throwException(interp);
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

unsigned TclObject::getListLength(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	int result;
	if (Tcl_ListObjLength(interp, obj, &result) != TCL_OK) {
		throwException(interp);
	}
	return result;
}

TclObject TclObject::getListIndex(Interpreter& interp_, unsigned index) const
{
	auto* interp = interp_.interp;
	Tcl_Obj* element;
	if (Tcl_ListObjIndex(interp, obj, index, &element) != TCL_OK) {
		throwException(interp);
	}
	return element ? TclObject(interp, element)
	               : TclObject(interp);
}

TclObject TclObject::getDictValue(Interpreter& interp_, const TclObject& key) const
{
	auto* interp = interp_.interp;
	Tcl_Obj* value;
	if (Tcl_DictObjGet(interp, obj, key.obj, &value) != TCL_OK) {
		throwException(interp);
	}
	return value ? TclObject(interp, value)
	             : TclObject(interp);
}

bool TclObject::evalBool(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	int result;
	if (Tcl_ExprBooleanObj(interp, obj, &result) != TCL_OK) {
		throwException(interp);
	}
	return result != 0;
}

string TclObject::executeCommand(Interpreter& interp_, bool compile)
{
	auto* interp = interp_.interp;
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
