#include "DebugCondition.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(TclObject command, TclObject condition)
	: BreakPointBase(command, condition)
	, id(++lastId)
{
}

} // namespace openmsx
