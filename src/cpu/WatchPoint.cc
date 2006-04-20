// $Id$

#include "WatchPoint.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned WatchPoint::lastId = 0;

WatchPoint::WatchPoint(CliComm& cliComm,
                       std::auto_ptr<TclObject> command,
                       std::auto_ptr<TclObject> condition,
                       Type type_, unsigned address_)
	: BreakPointBase(cliComm, command, condition)
	, type(type_), address(address_)
{
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

unsigned WatchPoint::getAddress() const
{
	return address;
}

} // namespace openmsx

