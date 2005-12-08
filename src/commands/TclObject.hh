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
	TclObject(Tcl_Interp* interp, const std::string& value);
	explicit TclObject(Tcl_Interp* interp);
	explicit TclObject(const TclObject& object);
	~TclObject();

	// get associated interpreter
	Tcl_Interp* getInterpreter() const;
	
	// value setters
	void setString(const std::string& value);
	void setInt(int value);
	void setDouble(double value);
	void setBinary(byte* buf, unsigned length);
	void addListElement(const std::string& element);
	void addListElement(int value);
	void addListElement(double value);
	void addListElement(TclObject& element);

	// value getters
	std::string getString() const;
	int getInt() const;
	double getDouble() const;
	const byte* getBinary(unsigned& length) const;
	unsigned getListLength() const;

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
	TclObject& operator==(const TclObject&);
	
	void init(Tcl_Obj* obj_);
	void unshare();
	void addListElement(Tcl_Obj* element);
	void parse(const char* str, int len, bool expression) const;

	Tcl_Interp* interp;
	Tcl_Obj* obj;
	bool owned;
};

} // namespace openmsx

#endif
