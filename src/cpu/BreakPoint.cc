#include "BreakPoint.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned BreakPoint::lastId = 0;

BreakPoint::BreakPoint(word address_, TclObject command, TclObject condition)
	: BreakPointBase(command, condition)
	, id(++lastId)
	, address(address_)
{
}

} // namespace openmsx
