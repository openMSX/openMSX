#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "TclObject.hh"
#include <string_view>

namespace openmsx {

class Interpreter;
class GlobalCliComm;

/** Base class for CPU break and watch points.
 */
class BreakPointBase
{
public:
	std::string_view getCondition() const { return condition.getString(); }
	std::string_view getCommand()   const { return command  .getString(); }
	TclObject getConditionObj() const { return condition; }
	TclObject getCommandObj()   const { return command; }
	bool onlyOnce() const { return once; }

	void checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp);

protected:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transfered to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	BreakPointBase(TclObject command_, TclObject condition_, bool once_)
		: command(std::move(command_))
		, condition(std::move(condition_))
		, once(once_) {}

private:
	bool isTrue(GlobalCliComm& cliComm, Interpreter& interp) const;

	TclObject command;
	TclObject condition;
	bool once;
	bool executing = false;
};

} // namespace openmsx

#endif
