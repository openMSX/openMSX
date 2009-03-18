// $Id$

#include "DebugCondition.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(CliComm& cliComm,
                               std::auto_ptr<TclObject> command,
                               std::auto_ptr<TclObject> condition)
	: BreakPointBase(cliComm, command, condition)
	, id(++lastId)
{
}

unsigned DebugCondition::getId() const
{
	return id;
}

} // namespace openmsx

