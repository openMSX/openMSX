#include "DebugCondition.hh"

namespace openmsx {

unsigned DebugCondition::lastId = 0;

DebugCondition::DebugCondition(TclObject command_, TclObject condition_, bool once_)
	: BreakPointBase(command_, condition_, once_)
	, id(++lastId)
{
}

} // namespace openmsx
