#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "TclObject.hh"

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
	[[nodiscard]] bool isEnabled() const { return enabled; }
	[[nodiscard]] bool onlyOnce() const { return once; }

	void setCondition(const TclObject& c) { condition = c; }
	void setCommand(const TclObject& c) { command = c; }
	void setEnabled(Interpreter& interp, const TclObject& e) {
		enabled = e.getBoolean(interp); // may throw
	}
	void setOnce(Interpreter& interp, const TclObject& o) {
		once = o.getBoolean(interp); // may throw
	}

	bool checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp);

protected:
	BreakPointBase() = default;
	BreakPointBase(TclObject command_, TclObject condition_, bool enabled_, bool once_)
		: command(std::move(command_))
		, condition(std::move(condition_))
		, enabled(enabled_), once(once_) {}

private:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transferred to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	[[nodiscard]] bool isTrue(GlobalCliComm& cliComm, Interpreter& interp) const;

private:
	TclObject command{"debug break"};
	TclObject condition;
	bool enabled = true;
	bool once = false;
	bool executing = false;
};

} // namespace openmsx

#endif
