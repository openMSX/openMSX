#include "BreakPointBase.hh"
#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "ScopedAssign.hh"

namespace openmsx {

bool BreakPointBase::isTrue(GlobalCliComm& cliComm, Interpreter& interp) const
{
	if (condition.getString().empty()) {
		// unconditional bp
		return true;
	}
	try {
		return condition.evalBool(interp);
	} catch (CommandException& e) {
		cliComm.printWarning(e.getMessage());
		return false;
	}
}

bool BreakPointBase::checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp)
{
	if (!enabled) return false;
	if (executing) {
		// no recursive execution
		return false;
	}
	ScopedAssign sa(executing, true);
	if (isTrue(cliComm, interp)) {
		try {
			command.executeCommand(interp, true); // compile command
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
		}
		return onlyOnce();
	}
	return false;
}

} // namespace openmsx
