#include "DebugCondition.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(TclObject command_, TclObject condition_)
	: BreakPointBase(command_, condition_)
	, id(++lastId)
{
}

} // namespace openmsx
