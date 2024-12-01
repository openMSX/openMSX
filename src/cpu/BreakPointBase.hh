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
	[[nodiscard]] TclObject getCondition() const { return condition; }
	[[nodiscard]] TclObject getCommand()   const { return command; }
	[[nodiscard]] bool onlyOnce() const { return once; }

	bool checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp);

protected:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transferred to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	BreakPointBase(TclObject command_, TclObject condition_, bool once_)
		: command(std::move(command_))
		, condition(std::move(condition_))
		, once(once_) {}

private:
	[[nodiscard]] bool isTrue(GlobalCliComm& cliComm, Interpreter& interp) const;

private:
	TclObject command;
	TclObject condition;
	bool once;
	bool executing = false;
};

} // namespace openmsx

#endif
