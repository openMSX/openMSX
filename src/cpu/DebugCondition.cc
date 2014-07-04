#include "DebugCondition.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(GlobalCliComm& cliComm, Interpreter& interp,
                               TclObject command, TclObject condition)
	: BreakPointBase(cliComm, interp, command, condition)
	, id(++lastId)
{
}

} // namespace openmsx
