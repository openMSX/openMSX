#include "BreakPoint.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned BreakPoint::lastId = 0;

BreakPoint::BreakPoint(word address_, TclObject command_, TclObject condition_, bool once_)
	: BreakPointBase(command_, condition_, once_)
	, id(++lastId)
	, address(address_)
{
}

} // namespace openmsx
