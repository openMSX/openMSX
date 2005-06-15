// $Id$

#ifndef TCLOBJECT_HH
#define TCLOBJECT_HH

#include <string>
#include "openmsx.hh"

class Tcl_Interp;
class Tcl_Obj;

namespace openmsx {

class TclObject
{
public:
	TclObject(Tcl_Interp* interp, Tcl_Obj* object);
	TclObject(Tcl_Interp* interp_, const std::string& value);
	TclObject(const TclObject& object);
	~TclObject();

	// setters
	void setString(const std::string& value);
	void setInt(int value);
	void setDouble(double value);
	void setBinary(byte* buf, unsigned length);
	void addListElement(const std::string& element);

	// getters
	std::string getString() const;
	int getInt() const;
	double getDouble() const;
	const byte* getBinary(unsigned& length) const;

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

private:
	void init(Tcl_Obj* obj_);
	void parse(const char* str, int len, bool expression) const;

	Tcl_Interp* interp;
	Tcl_Obj* obj;
	bool owned;
};

} // namespace openmsx

#endif
