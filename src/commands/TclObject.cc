#include "TclObject.hh"
#include "Interpreter.hh"
#include "CommandException.hh"
#include "narrow.hh"

namespace openmsx {

[[noreturn]] static void throwException(Tcl_Interp* interp)
{
	std::string_view message = interp ? Tcl_GetStringResult(interp)
	                            : "TclObject error";
	throw CommandException(message);
}

static void unshare(Tcl_Obj*& obj)
{
	if (Tcl_IsShared(obj)) {
		Tcl_DecrRefCount(obj);
		obj = Tcl_DuplicateObj(obj);
		Tcl_IncrRefCount(obj);
	}
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
	unshare(obj);
	Tcl_Interp* interp = nullptr;
	if (Tcl_ListObjAppendElement(interp, obj, element) != TCL_OK) {
		throwException(interp);
	}
}

void TclObject::addListElementsImpl(std::initializer_list<Tcl_Obj*> l)
{
	addListElementsImpl(int(l.size()), l.begin());
}

void TclObject::addListElementsImpl(int objc, Tcl_Obj* const* objv)
{
	unshare(obj);
	Tcl_Interp* interp = nullptr; // see comment in addListElement
	if (Tcl_ListObjReplace(interp, obj, INT_MAX, 0, objc, objv) != TCL_OK) {
		throwException(interp);
	}
}

void TclObject::addDictKeyValues(std::initializer_list<Tcl_Obj*> keyValuePairs)
{
	assert((keyValuePairs.size() % 2) == 0);
	unshare(obj);
	Tcl_Interp* interp = nullptr; // see comment in addListElement
	auto it = keyValuePairs.begin(), et = keyValuePairs.end();
	while (it != et) {
		Tcl_Obj* key   = *it++;
		Tcl_Obj* value = *it++;
		if (Tcl_DictObjPut(interp, obj, key, value) != TCL_OK) {
			throwException(interp);
		}
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

std::optional<int> TclObject::getOptionalInt() const
{
	int result;
	if (Tcl_GetIntFromObj(nullptr, obj, &result) != TCL_OK) {
		return {};
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

std::optional<bool> TclObject::getOptionalBool() const
{
	int result;
	if (Tcl_GetBooleanFromObj(nullptr, obj, &result) != TCL_OK) {
		return {};
	}
	return result != 0;
}

float TclObject::getFloat(Interpreter& interp_) const
{
	// Tcl doesn't directly support 'float', only 'double', so use that.
	// But we hide this from the rest from the code, and we do the
	// narrowing conversion in only this single location.
	return narrow_cast<float>(getDouble(interp_));
}
std::optional<float> TclObject::getOptionalFloat() const
{
	if (auto d = getOptionalDouble()) {
		return narrow_cast<float>(*d);
	}
	return {};
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

std::optional<double> TclObject::getOptionalDouble() const
{
	double result;
	if (Tcl_GetDoubleFromObj(nullptr, obj, &result) != TCL_OK) {
		return {};
	}
	return result;
}

zstring_view TclObject::getString() const
{
	int length;
	char* buf = Tcl_GetStringFromObj(obj, &length);
	return {buf, size_t(length)};
}

std::span<const uint8_t> TclObject::getBinary() const
{
	int length;
	auto* buf = Tcl_GetByteArrayFromObj(obj, &length);
	return {buf, size_t(length)};
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
unsigned TclObject::getListLengthUnchecked() const
{
	int result;
	if (Tcl_ListObjLength(nullptr, obj, &result) != TCL_OK) {
		return 0; // error
	}
	return result;
}

TclObject TclObject::getListIndex(Interpreter& interp_, unsigned index) const
{
	auto* interp = interp_.interp;
	Tcl_Obj* element;
	if (Tcl_ListObjIndex(interp, obj, narrow<int>(index), &element) != TCL_OK) {
		throwException(interp);
	}
	return element ? TclObject(element) : TclObject();
}
TclObject TclObject::getListIndexUnchecked(unsigned index) const
{
	Tcl_Obj* element;
	if (Tcl_ListObjIndex(nullptr, obj, narrow<int>(index), &element) != TCL_OK) {
		return {};
	}
	return element ? TclObject(element) : TclObject();
}

void TclObject::removeListIndex(Interpreter& interp_, unsigned index)
{
	unshare(obj);
	auto* interp = interp_.interp;
	if (Tcl_ListObjReplace(interp, obj, narrow<int>(index), 1, 0, nullptr) != TCL_OK) {
		throwException(interp);
	}
}

void TclObject::setDictValue(Interpreter& interp_, const TclObject& key, const TclObject& value)
{
	unshare(obj);
	auto* interp = interp_.interp;
	if (Tcl_DictObjPut(interp, obj, key.obj, value.obj) != TCL_OK) {
		throwException(interp);
	}
}

TclObject TclObject::getDictValue(Interpreter& interp_, const TclObject& key) const
{
	auto* interp = interp_.interp;
	Tcl_Obj* value;
	if (Tcl_DictObjGet(interp, obj, key.obj, &value) != TCL_OK) {
		throwException(interp);
	}
	return value ? TclObject(value) : TclObject();
}

std::optional<TclObject> TclObject::getOptionalDictValue(const TclObject& key) const
{
	Tcl_Obj* value;
	if ((Tcl_DictObjGet(nullptr, obj, key.obj, &value) != TCL_OK) || !value) {
		return {};
	}
	return TclObject(value);
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

TclObject TclObject::eval(Interpreter& interp_) const
{
	auto* interp = interp_.interp;
	Tcl_Obj* result;
	if (Tcl_ExprObj(interp, obj, &result) != TCL_OK) {
		throwException(interp);
	}
	return TclObject(result);
}

TclObject TclObject::executeCommand(Interpreter& interp_, bool compile)
{
	auto* interp = interp_.interp;
	if (int flags = compile ? 0 : TCL_EVAL_DIRECT;
	    Tcl_EvalObjEx(interp, obj, flags) != TCL_OK) {
		throw CommandException(Tcl_GetStringResult(interp));
	}
	return TclObject(Tcl_GetObjResult(interp));
}

} // namespace openmsx
