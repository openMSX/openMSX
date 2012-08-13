// $Id$

#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "TclObject.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <memory>

struct Tcl_Interp;

namespace openmsx {

class GlobalCliComm;

/** Base class for CPU break and watch points.
 */
class BreakPointBase : private noncopyable
{
public:
	string_ref getCondition() const { return condition.getString(); }
	string_ref getCommand()   const { return command  .getString(); }
	TclObject getConditionObj() const { return condition; }
	TclObject getCommandObj()   const { return command; }

	void checkAndExecute();

	// get associated interpreter
	Tcl_Interp* getInterpreter() const;

protected:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transfered to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	BreakPointBase(GlobalCliComm& cliComm,
	               TclObject command, TclObject condition);

private:
	bool isTrue() const;

	GlobalCliComm& cliComm;
	TclObject command;
	TclObject condition;
	bool executing;
};

} // namespace openmsx

#endif
