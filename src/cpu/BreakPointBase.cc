// $Id$

#include "BreakPointBase.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXCliComm.hh"
#include "ScopedAssign.hh"

namespace openmsx {

BreakPointBase::BreakPointBase(MSXCliComm& cliComm_,
                               std::auto_ptr<TclObject> command_,
                               std::auto_ptr<TclObject> condition_)
	: cliComm(cliComm_), command(command_), condition(condition_)
	, executing(false)
{
}

BreakPointBase::~BreakPointBase()
{
}

std::string BreakPointBase::getCondition() const
{
	return condition.get() ? condition->getString() : "";
}

std::string BreakPointBase::getCommand() const
{
	return command->getString();
}

bool BreakPointBase::isTrue() const
{
	if (!condition.get()) {
		// unconditional bp
		return true;
	}
	try {
		return condition->evalBool();
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
			command->executeCommand(true); // compile command
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

} // namespace openmsx

