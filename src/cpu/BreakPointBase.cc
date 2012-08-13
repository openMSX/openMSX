// $Id$

#include "BreakPointBase.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "GlobalCliComm.hh"
#include "ScopedAssign.hh"

namespace openmsx {

BreakPointBase::BreakPointBase(GlobalCliComm& cliComm_,
                               TclObject command_, TclObject condition_)
	: cliComm(cliComm_), command(command_), condition(condition_)
	, executing(false)
{
}

bool BreakPointBase::isTrue() const
{
	if (condition.getString().empty()) {
		// unconditional bp
		return true;
	}
	try {
		return condition.evalBool();
	} catch (CommandException& e) {
		cliComm.printWarning(e.getMessage());
		return false;
	}
}

void BreakPointBase::checkAndExecute()
{
	if (executing) {
		// no recursive execution
		return;
	}
	ScopedAssign<bool> sa(executing, true);
	if (isTrue()) {
		try {
			command.executeCommand(true); // compile command
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

Tcl_Interp* BreakPointBase::getInterpreter() const
{
	return command.getInterpreter();
}

} // namespace openmsx

