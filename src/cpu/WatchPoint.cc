// $Id$

#include "WatchPoint.hh"
#include "TclObject.hh"
#include <cassert>

namespace openmsx {

unsigned WatchPoint::lastId = 0;

WatchPoint::WatchPoint(MSXCliComm& cliComm,
                       std::auto_ptr<TclObject> command,
                       std::auto_ptr<TclObject> condition,
                       Type type_, unsigned beginAddr_, unsigned endAddr_)
	: BreakPointBase(cliComm, command, condition)
	, type(type_), beginAddr(beginAddr_), endAddr(endAddr_)
{
	assert(beginAddr <= endAddr);
	id = ++lastId;
}

WatchPoint::~WatchPoint()
{
}

unsigned WatchPoint::getId() const
{
	return id;
}

WatchPoint::Type WatchPoint::getType() const
{
	return type;
}

unsigned WatchPoint::getBeginAddress() const
{
	return beginAddr;
}

unsigned WatchPoint::getEndAddress() const
{
	return endAddr;
}

} // namespace openmsx

