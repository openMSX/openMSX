#ifndef TCLOBJECT_HH
#define TCLOBJECT_HH

#include "string_ref.hh"
#include "openmsx.hh"
#include <iterator>

struct Tcl_Interp;
struct Tcl_Obj;

namespace openmsx {

class Interpreter;

class TclObject
{
public:
	TclObject(Tcl_Interp* interp, Tcl_Obj* object);
	TclObject(Tcl_Interp* interp, string_ref value);
	TclObject(Interpreter& interp, string_ref value);
	explicit TclObject(string_ref value);
	explicit TclObject(Tcl_Interp* interp);
	explicit TclObject(Interpreter& interp);
	TclObject(const TclObject& object);
	TclObject();
	~TclObject();

	// assignment operator so we can use vector<TclObject>
	TclObject& operator=(const TclObject& other);

	// get associated interpreter
	Tcl_Interp* getInterpreter() const;
	// get underlying Tcl_Obj
	Tcl_Obj* getTclObject();

	// value setters
	void setString(string_ref value);
	void setInt(int value);
	void setBoolean(bool value);
	void setDouble(double value);
	void setBinary(byte* buf, unsigned length);
	void addListElement(string_ref element);
	void addListElement(int value);
	void addListElement(double value);
	void addListElement(const TclObject& element);
	template <typename ITER> void addListElements(ITER begin, ITER end);
	template <typename CONT> void addListElements(const CONT& container);

	// value getters
	string_ref getString() const;
	int getInt() const;
	bool getBoolean() const;
	double getDouble() const;
	const byte* getBinary(unsigned& length) const;
	unsigned getListLength() const;
	TclObject getListIndex(unsigned index) const;
	TclObject getDictValue(const TclObject& key) const;

	// expressions
	bool evalBool() const;

	/** Interpret the content of this TclObject as an expression and
	  * check for syntax errors. Throws CommandException in case of
	  * an error.
	  * Note: this method does not catch all errors (some are only
	  * found while evaluating the expression) but it's nice as a
	  * quick check for typos.
	  */
	void checkExpression() const;
	void checkCommand() const;

	/** Interpret this TclObject as a command and execute it.
	  * @param compile Should the command be compiled to bytecode? The
	  *           bytecode is stored inside the TclObject can speed up
	  *           future invocations of the same command. Only set this
	  *           flag when the command will be executed more than once.
	  * TODO return TclObject instead of string?
	  */
	std::string executeCommand(bool compile = false);

	/** Comparison. Only compares the 'value', not the interpreter. */
	bool operator==(const TclObject& other) const {
		return getString() == other.getString();
	}
	bool operator!=(const TclObject& other) const {
		return !(*this == other);
	}

private:
	void init(Tcl_Obj* obj_);
	void throwException() const;
	void addListElement(Tcl_Obj* element);
	void parse(const char* str, int len, bool expression) const;

	Tcl_Interp* interp;
	Tcl_Obj* obj;
};

template <typename ITER>
void TclObject::addListElements(ITER begin, ITER end)
{
	for (ITER it = begin; it != end; ++it) {
		addListElement(*it);
	}
}

template <typename CONT>
void TclObject::addListElements(const CONT& container)
{
	addListElements(std::begin(container), std::end(container));
}

} // namespace openmsx

#endif
