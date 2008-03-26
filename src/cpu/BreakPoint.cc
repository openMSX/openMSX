// $Id$

#include "BreakPoint.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned BreakPoint::lastId = 0;

BreakPoint::BreakPoint(CliComm& cliComm, word address_,
                       std::auto_ptr<TclObject> command,
                       std::auto_ptr<TclObject> condition)
	: BreakPointBase(cliComm, command, condition)
	, address(address_)
{
	id = ++lastId;
}

word BreakPoint::getAddress() const
{
	return address;
}

unsigned BreakPoint::getId() const
{
	return id;
}

} // namespace openmsx

