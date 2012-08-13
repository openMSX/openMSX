// $Id$

#include "DebugCondition.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(GlobalCliComm& cliComm,
                               TclObject command, TclObject condition)
	: BreakPointBase(cliComm, command, condition)
	, id(++lastId)
{
}

} // namespace openmsx

