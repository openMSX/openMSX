#include "WatchPoint.hh"
#include <cassert>

namespace openmsx {

unsigned WatchPoint::lastId = 0;

WatchPoint::WatchPoint(TclObject command_, TclObject condition_,
                       Type type_, unsigned beginAddr_, unsigned endAddr_,
                       unsigned newId /*= -1*/)
	: BreakPointBase(command_, condition_)
	, id((newId == unsigned(-1)) ? ++lastId : newId)
	, beginAddr(beginAddr_), endAddr(endAddr_), type(type_)
{
	assert(beginAddr <= endAddr);
}

WatchPoint::~WatchPoint()
{
}

} // namespace openmsx
