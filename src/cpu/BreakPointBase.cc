#include "BreakPointBase.hh"
#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "ScopedAssign.hh"

namespace openmsx {

BreakPointBase::BreakPointBase(TclObject command_, TclObject condition_)
	: command(command_), condition(condition_)
	, executing(false)
{
}

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

void BreakPointBase::checkAndExecute(GlobalCliComm& cliComm, Interpreter& interp)
{
	if (executing) {
		// no recursive execution
		return;
	}
	ScopedAssign<bool> sa(executing, true);
	if (isTrue(cliComm, interp)) {
		try {
			command.executeCommand(interp, true); // compile command
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

} // namespace openmsx
