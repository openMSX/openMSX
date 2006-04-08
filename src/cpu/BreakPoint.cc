// $Id$

#include "BreakPoint.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "CliComm.hh"

namespace openmsx {

unsigned BreakPoint::lastId = 0;

BreakPoint::BreakPoint(CliComm& cliComm_, word address_,
                       std::auto_ptr<TclObject> command_,
                       std::auto_ptr<TclObject> condition_)
	: cliComm(cliComm_), command(command_)
	, condition(condition_), address(address_)
{
	id = ++lastId;
}

BreakPoint::~BreakPoint()
{
}

word BreakPoint::getAddress() const
{
	return address;
}

std::string BreakPoint::getCondition() const
{
	return condition.get() ? condition->getString() : "";
}

std::string BreakPoint::getCommand() const
{
	return command->getString();
}

unsigned BreakPoint::getId() const
{
	return id;
}

bool BreakPoint::isTrue() const
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

void BreakPoint::checkAndExecute()
{
	if (isTrue()) {
		try {
			command->executeCommand(true); // compile command
		} catch (CommandException& e) {
			cliComm.printWarning(e.getMessage());
		}
	}
}

} // namespace openmsx

