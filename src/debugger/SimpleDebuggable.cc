// $Id$

#include "SimpleDebuggable.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include <cassert>


namespace openmsx {

SimpleDebuggable::SimpleDebuggable(
		MSXMotherBoard& motherBoard_, const std::string& name_,
		const std::string& description_, unsigned size_)
	: motherBoard(motherBoard_)
	, name(name_)
	, description(description_)
	, size(size_)
{
	motherBoard.getDebugger().registerDebuggable(name, *this);
}

SimpleDebuggable::~SimpleDebuggable()
{
	motherBoard.getDebugger().unregisterDebuggable(name, *this);
}

unsigned SimpleDebuggable::getSize() const
{
	return size;
}

const std::string& SimpleDebuggable::getDescription() const
{
	return description;
}

byte SimpleDebuggable::read(unsigned address)
{
	return read(address, motherBoard.getScheduler().getCurrentTime());
}

byte SimpleDebuggable::read(unsigned /*address*/, const EmuTime& /*time*/)
{
	assert(false);
	return 0;
}

void SimpleDebuggable::write(unsigned address, byte value)
{
	write(address, value, motherBoard.getScheduler().getCurrentTime());
}

void SimpleDebuggable::write(unsigned /*address*/, byte /*value*/,
                             const EmuTime& /*time*/)
{
	// does nothing
}

} // namespace openmsx
