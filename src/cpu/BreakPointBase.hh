#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "TclObject.hh"
#include "string_view.hh"

namespace openmsx {

class Interpreter;
class GlobalCliComm;

/** Base class for CPU break and watch points.
 */
class BreakPointBase
{
public:
	string_view getCondition() const { return condition.getString(); }
	string_view getCommand()   const { return command  .getString(); }
	TclObject getConditionObj() const { return condition; }
	TclObject getCommandObj()   const { return command; }

	void checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp);

protected:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transfered to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	BreakPointBase(TclObject command, TclObject condition);

private:
	bool isTrue(GlobalCliComm& cliComm, Interpreter& interp) const;

	TclObject command;
	TclObject condition;
	bool executing;
};

} // namespace openmsx

#endif
